/*
 * smart_parser.c - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "smart_parser.h"
#include "smart_attributes.h"
#include <string.h>
#include <stdio.h>

uint64_t smart_raw_value_to_uint64(const uint8_t* raw_value) {
    uint64_t result = 0;

    /* SMART raw values are 6 bytes, stored little-endian */
    for (int i = 0; i < 6; i++) {
        result |= ((uint64_t)raw_value[i]) << (i * 8);
    }

    return result;
}

int smart_parse_values(const uint8_t* raw_data, smart_values_page_t* values) {
    if (raw_data == NULL || values == NULL) {
        return -1;
    }

    /* Copy the entire structure - it's packed and matches the on-disk format */
    memcpy(values, raw_data, sizeof(smart_values_page_t));

    return 0;
}

int smart_parse_thresholds(const uint8_t* raw_data, smart_thresholds_page_t* thresholds) {
    if (raw_data == NULL || thresholds == NULL) {
        return -1;
    }

    /* Copy the entire structure - it's packed and matches the on-disk format */
    memcpy(thresholds, raw_data, sizeof(smart_thresholds_page_t));

    return 0;
}

attribute_health_status_t assess_attribute_health(const parsed_smart_attribute_t* attr) {
    if (attr == NULL) {
        return ATTR_STATUS_UNKNOWN;
    }

    /* Special handling for temperature (0xC2, 0xBE, 0xE7) */
    if (attr->id == 0xC2 || attr->id == 0xBE || attr->id == 0xE7) {
        uint8_t temp = (uint8_t)attr->raw_value;  // Temperature is in lowest byte
        if (temp >= 60) {
            return ATTR_STATUS_CRITICAL;
        } else if (temp >= 50) {
            return ATTR_STATUS_WARNING;
        }
        return ATTR_STATUS_GOOD;
    }

    /* Critical attributes with non-zero raw values are problems */
    if (attr->is_critical) {
        /* Reallocated sectors, pending sectors, uncorrectable sectors */
        if (attr->id == 0x05 || attr->id == 0xC5 || attr->id == 0xC6 ||
            attr->id == 0xBB || attr->id == 0xB8) {
            if (attr->raw_value > 0) {
                /* Pending sectors are warnings, actual bad sectors are critical */
                if (attr->id == 0xC5) {
                    return ATTR_STATUS_WARNING;
                }
                return ATTR_STATUS_CRITICAL;
            }
        }

        /* Spin retry count - even one is bad */
        if (attr->id == 0x0A && attr->raw_value > 0) {
            return ATTR_STATUS_CRITICAL;
        }

        /* Reallocation event count - warnings */
        if (attr->id == 0xC4 && attr->raw_value > 0) {
            return ATTR_STATUS_WARNING;
        }
    }

    /* Check current value against threshold */
    if (attr->threshold > 0) {
        if (attr->current_value <= attr->threshold) {
            /* At or below threshold is critical */
            return ATTR_STATUS_CRITICAL;
        } else if (attr->current_value <= attr->threshold + 10) {
            /* Within 10 of threshold is warning */
            return ATTR_STATUS_WARNING;
        }
    }

    return ATTR_STATUS_GOOD;
}

disk_health_status_t assess_overall_health(disk_smart_data_t* data) {
    if (data == NULL || !data->is_present) {
        return DISK_STATUS_ERROR;
    }

    disk_health_status_t overall = DISK_STATUS_GOOD;

    /* Check each attribute and take the worst status */
    for (int i = 0; i < data->num_attributes; i++) {
        attribute_health_status_t attr_status = assess_attribute_health(&data->attributes[i]);
        data->attributes[i].status = attr_status;

        /* Update overall status to worst seen */
        if (attr_status == ATTR_STATUS_CRITICAL) {
            overall = DISK_STATUS_CRITICAL;
        } else if (attr_status == ATTR_STATUS_WARNING && overall == DISK_STATUS_GOOD) {
            overall = DISK_STATUS_WARNING;
        }
    }

    data->overall_status = overall;
    return overall;
}

int smart_combine_data(int disk_num, const char* disk_name,
                       const smart_values_page_t* values,
                       const smart_thresholds_page_t* thresholds,
                       disk_smart_data_t* data) {
    if (values == NULL || thresholds == NULL || data == NULL) {
        return -1;
    }

    /* Initialize the data structure */
    memset(data, 0, sizeof(disk_smart_data_t));
    data->disk_number = disk_num;
    data->is_present = 1;

    if (disk_name != NULL) {
        strncpy(data->disk_name, disk_name, sizeof(data->disk_name) - 1);
        data->disk_name[sizeof(data->disk_name) - 1] = '\0';
    }

    /* Parse each attribute */
    int attr_count = 0;
    for (int i = 0; i < 30 && attr_count < MAX_SMART_ATTRIBUTES; i++) {
        const smart_attribute_t* attr = &values->attributes[i];

        /* Skip invalid/empty attributes (ID 0x00) */
        if (attr->id == 0x00) {
            continue;
        }

        /* Find corresponding threshold */
        uint8_t threshold = 0;
        for (int j = 0; j < 30; j++) {
            if (thresholds->thresholds[j].id == attr->id) {
                threshold = thresholds->thresholds[j].threshold;
                break;
            }
        }

        /* Get attribute definition */
        const smart_attribute_def_t* def = get_attribute_definition(attr->id);

        /* Fill in parsed attribute */
        parsed_smart_attribute_t* parsed = &data->attributes[attr_count];
        parsed->id = attr->id;
        parsed->name = (def != NULL) ? def->name : "Unknown_Attribute";
        parsed->current_value = attr->current_value;
        parsed->worst_value = attr->worst_value;
        parsed->threshold = threshold;
        parsed->raw_value = smart_raw_value_to_uint64(attr->raw_value);
        parsed->is_critical = (def != NULL) ? def->is_critical : 0;
        parsed->status = ATTR_STATUS_UNKNOWN;  // Will be assessed later

        attr_count++;
    }

    data->num_attributes = attr_count;

    /* If no valid attributes were found, mark disk as not present */
    if (attr_count == 0) {
        data->is_present = 0;
        data->overall_status = DISK_STATUS_ERROR;
        return -1;
    }

    /* Check if this looks like a real disk. Real disks must have Power On Hours (0x09)
     * with a reasonable value (< 10 years = 87600 hours). Empty slots return garbage. */
    int has_power_on_hours = 0;
    for (int i = 0; i < data->num_attributes; i++) {
        if (data->attributes[i].id == 0x09) {
            /* Power On Hours should be reasonable (< 87600 = 10 years) */
            if (data->attributes[i].raw_value < 87600) {
                has_power_on_hours = 1;
            }
            break;
        }
    }

    if (!has_power_on_hours) {
        /* No valid Power On Hours - this is probably an empty slot */
        data->is_present = 0;
        data->overall_status = DISK_STATUS_ERROR;
        return -1;
    }

    /* Assess overall health */
    assess_overall_health(data);

    return 0;
}
