/*
 * output_formatter.c - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "output_formatter.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

const char* disk_status_string(disk_health_status_t status) {
    switch (status) {
        case DISK_STATUS_PASSED:   return "PASSED";
        case DISK_STATUS_FAILED:   return "FAILED";
        case DISK_STATUS_ERROR:    return "ERROR";
        default:                   return "UNKNOWN";
    }
}

const char* attribute_status_string(attribute_health_status_t status) {
    switch (status) {
        case ATTR_STATUS_PASSED:     return "OK";
        case ATTR_STATUS_FAILED:   return "FAILED";
        case ATTR_STATUS_UNKNOWN:  return "UNKNOWN";
        default:                   return "UNKNOWN";
    }
}

/* Helper: Get temperature from attributes */
static int get_temperature(const disk_smart_data_t* disk) {
    for (int i = 0; i < disk->num_attributes; i++) {
        if (disk->attributes[i].id == 0xC2 ||
            disk->attributes[i].id == 0xBE ||
            disk->attributes[i].id == 0xE7) {
            return (int)(disk->attributes[i].raw_value & 0xFF);
        }
    }
    return -1;  // Not found
}

/* Helper: Get power-on hours from attributes */
static uint64_t get_power_on_hours(const disk_smart_data_t* disk) {
    for (int i = 0; i < disk->num_attributes; i++) {
        if (disk->attributes[i].id == 0x09 || disk->attributes[i].id == 0xE9) {
            return disk->attributes[i].raw_value;
        }
    }
    return 0;
}

/* Helper: Get power cycle count from attributes */
static uint64_t get_power_cycles(const disk_smart_data_t* disk) {
    for (int i = 0; i < disk->num_attributes; i++) {
        if (disk->attributes[i].id == 0x0C) {
            return disk->attributes[i].raw_value;
        }
    }
    return 0;
}

/* Helper: Count critical errors */
static int count_critical_errors(const disk_smart_data_t* disk, const char** error_msg) {
    int error_count = 0;
    static char msg_buf[256];
    msg_buf[0] = '\0';

    for (int i = 0; i < disk->num_attributes; i++) {
        const parsed_smart_attribute_t* attr = &disk->attributes[i];

        if (attr->id == 0x05 && attr->raw_value > 0) {  // Reallocated sectors
            error_count++;
            snprintf(msg_buf + strlen(msg_buf), sizeof(msg_buf) - strlen(msg_buf),
                     "  Reallocated Sectors: %llu\n", (unsigned long long)attr->raw_value);
        } else if (attr->id == 0xC5 && attr->raw_value > 0) {  // Pending sectors
            error_count++;
            snprintf(msg_buf + strlen(msg_buf), sizeof(msg_buf) - strlen(msg_buf),
                     "  Current Pending Sectors: %llu\n", (unsigned long long)attr->raw_value);
        } else if (attr->id == 0xC6 && attr->raw_value > 0) {  // Uncorrectable
            error_count++;
            snprintf(msg_buf + strlen(msg_buf), sizeof(msg_buf) - strlen(msg_buf),
                     "  Uncorrectable Sectors: %llu\n", (unsigned long long)attr->raw_value);
        }
    }

    *error_msg = (msg_buf[0] != '\0') ? msg_buf : NULL;
    return error_count;
}

void format_summary(const char* device_path, const disk_smart_data_t* disks, int num_disks, const char* controller_model) {
    printf("jmraidstatus v1.0 - SMART Health Monitor\n");
    printf("Device: %s", device_path);
    if (controller_model != NULL && controller_model[0] != '\0') {
        printf(" (Controller: %s)", controller_model);
    }
    printf("\n\n");

    (void)num_disks;  /* Unused but kept for API consistency */

    int failed_count = 0;

    /* Display each disk */
    for (int i = 0; i < 5; i++) {
        if (!disks[i].is_present) {
            continue;
        }

        printf("Disk %d: %s\n", i, disks[i].disk_name[0] ? disks[i].disk_name : "Unknown");

        /* Serial number */
        if (disks[i].serial_number[0] != '\0') {
            printf("  Serial: %s\n", disks[i].serial_number);
        }

        /* Firmware revision */
        if (disks[i].firmware_rev[0] != '\0') {
            printf("  Firmware: %s\n", disks[i].firmware_rev);
        }

        /* Disk size (if available) */
        if (disks[i].size_mb > 0) {
            /* 1 TB = 1024 * 1024 MB = 1,048,576 MB */
            if (disks[i].size_mb >= 1048576) {
                printf("  Size: %.1f TB\n", disks[i].size_mb / 1024.0 / 1024.0);
            } else {
                printf("  Size: %llu GB\n", (unsigned long long)(disks[i].size_mb / 1024));
            }
        }

        printf("  Status: %s\n", disk_status_string(disks[i].overall_status));

        /* Temperature */
        int temp = get_temperature(&disks[i]);
        if (temp >= 0) {
            printf("  Temperature: %d°C\n", temp);
        }

        /* Power on hours */
        uint64_t hours = get_power_on_hours(&disks[i]);
        if (hours > 0) {
            printf("  Power On Hours: %llu hours (%llu days)\n",
                   (unsigned long long)hours, (unsigned long long)(hours / 24));
        }

        /* Power cycles */
        uint64_t cycles = get_power_cycles(&disks[i]);
        if (cycles > 0) {
            printf("  Power Cycles: %llu\n", (unsigned long long)cycles);
        }

        /* Check for errors */
        const char* error_msg = NULL;
        int errors = count_critical_errors(&disks[i], &error_msg);

        if (errors == 0) {
            printf("  No errors detected\n");
        } else {
            printf("%s", error_msg);
            if (disks[i].overall_status == DISK_STATUS_FAILED) {
                printf("  FAILED: Disk may be failing - backup data immediately!\n");
            }
        }

        printf("\n");

        /* Count for overall status */
        if (disks[i].overall_status == DISK_STATUS_FAILED) {
            failed_count++;
        }
    }

    /* Overall SMART health */
    printf("Overall SMART Health: ");
    if (failed_count > 0) {
        printf("FAILED - Check disk(s) immediately!\n");
    } else {
        printf("PASSED - All disks healthy\n");
    }
}

void format_full_smart(const disk_smart_data_t* disk) {
    if (!disk->is_present) {
        printf("Disk %d: Not present\n", disk->disk_number);
        return;
    }

    printf("Disk %d: %s\n", disk->disk_number,
           disk->disk_name[0] ? disk->disk_name : "Unknown");

    /* Serial number */
    if (disk->serial_number[0] != '\0') {
        printf("Serial: %s\n", disk->serial_number);
    }

    /* Firmware revision */
    if (disk->firmware_rev[0] != '\0') {
        printf("Firmware: %s\n", disk->firmware_rev);
    }

    /* Disk size (if available) */
    if (disk->size_mb > 0) {
        /* 1 TB = 1024 * 1024 MB = 1,048,576 MB */
        if (disk->size_mb >= 1048576) {
            printf("Size: %.1f TB\n", disk->size_mb / 1024.0 / 1024.0);
        } else {
            printf("Size: %llu GB\n", (unsigned long long)(disk->size_mb / 1024));
        }
    }

    printf("Status: %s\n\n", disk_status_string(disk->overall_status));

    printf("SMART Attributes:\n");
    printf("ID  Name                        Value Worst Thresh Raw Value      Status\n");
    printf("--------------------------------------------------------------------------------\n");

    for (int i = 0; i < disk->num_attributes; i++) {
        const parsed_smart_attribute_t* attr = &disk->attributes[i];

        /* For temperature attributes, only show the first byte of raw value */
        uint64_t display_raw = attr->raw_value;
        if (attr->id == 0xC2 || attr->id == 0xBE || attr->id == 0xE7) {
            display_raw = attr->raw_value & 0xFF;
        }

        printf("%02X  %-28s %3d   %3d   %3d    %-15llu %s",
               attr->id,
               attr->name,
               attr->current_value,
               attr->worst_value,
               attr->threshold,
               (unsigned long long)display_raw,
               attribute_status_string(attr->status));

        if (attr->is_critical) {
            printf(" [Critical]");
        }

        printf("\n");
    }

    printf("\nHealth Assessment:\n");

    /* Check specific critical attributes */
    int has_issues = 0;

    for (int i = 0; i < disk->num_attributes; i++) {
        const parsed_smart_attribute_t* attr = &disk->attributes[i];

        if (attr->id == 0x05) {  // Reallocated sectors
            if (attr->raw_value == 0) {
                printf("  PASSED: No reallocated sectors\n");
            } else {
                printf("  FAILED: %llu reallocated sectors\n", (unsigned long long)attr->raw_value);
                has_issues = 1;
            }
        } else if (attr->id == 0xC5) {  // Pending sectors
            if (attr->raw_value == 0) {
                printf("  PASSED: No pending sectors\n");
            } else {
                printf("  FAILED: %llu pending sectors\n", (unsigned long long)attr->raw_value);
                has_issues = 1;
            }
        } else if (attr->id == 0xC6) {  // Uncorrectable sectors
            if (attr->raw_value == 0) {
                printf("  PASSED: No uncorrectable sectors\n");
            } else {
                printf("  FAILED: %llu uncorrectable sectors\n", (unsigned long long)attr->raw_value);
                has_issues = 1;
            }
        } else if (attr->id == 0xC2) {  // Temperature
            int temp = (int)(attr->raw_value & 0xFF);
            if (temp < 60) {
                printf("  PASSED: Temperature within normal range (%d°C)\n", temp);
            } else {
                printf("  FAILED: Temperature too high (%d°C)\n", temp);
                has_issues = 1;
            }
        }
    }

    if (!has_issues) {
        printf("  All critical parameters within acceptable range\n");
    }

    printf("\n");
}

void format_json(const char* device_path, const disk_smart_data_t* disks, int num_disks,
                 int expected_array_size, int present_disks, int is_degraded,
                 const char* controller_model) {
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);

    (void)num_disks;  /* Unused but kept for API consistency */

    /* Determine overall RAID status */
    const char* raid_status = "unknown";
    int has_failed_disk = 0;

    /* Check for failed disks */
    for (int i = 0; i < 5; i++) {
        if (disks[i].is_present && disks[i].overall_status == DISK_STATUS_FAILED) {
            has_failed_disk = 1;
            break;
        }
    }

    /* Determine status based on array size and disk health */
    if (expected_array_size > 0 && present_disks > 0) {
        if (is_degraded) {
            raid_status = "degraded";
        } else if (present_disks > expected_array_size) {
            raid_status = has_failed_disk ? "failed" : "oversized";
        } else if (has_failed_disk) {
            raid_status = "failed";
        } else {
            raid_status = "healthy";
        }
    } else {
        /* No array size validation */
        raid_status = has_failed_disk ? "failed" : "healthy";
    }

    printf("{\n");
    printf("  \"version\": \"1.0\",\n");
    printf("  \"backend\": \"jmicron\",\n");
    printf("  \"device\": \"%s\",\n", device_path);
    printf("  \"timestamp\": \"%s\",\n", timestamp);

    /* Controller information */
    printf("  \"controller\": {\n");
    printf("    \"model\": \"%s\",\n", controller_model ? controller_model : "Unknown");
    printf("    \"type\": \"raid_array\"\n");
    printf("  },\n");

    /* RAID status section */
    printf("  \"raid_status\": {\n");
    printf("    \"status\": \"%s\",\n", raid_status);
    if (expected_array_size > 0) {
        printf("    \"expected_disks\": %d,\n", expected_array_size);
    }
    if (present_disks > 0) {
        printf("    \"present_disks\": %d,\n", present_disks);
    }
    printf("    \"rebuilding\": false,\n");  /* Not yet detected */

    /* Build issues array */
    printf("    \"issues\": [");
    int first_issue = 1;

    if (is_degraded && expected_array_size > 0 && present_disks > 0) {
        if (!first_issue) printf(", ");
        printf("\n      \"Degraded: Expected %d disk%s but found only %d disk%s\"",
               expected_array_size, expected_array_size == 1 ? "" : "s",
               present_disks, present_disks == 1 ? "" : "s");
        first_issue = 0;
    } else if (present_disks > expected_array_size && expected_array_size > 0) {
        if (!first_issue) printf(", ");
        printf("\n      \"Oversized: Expected %d disk%s but found %d disk%s\"",
               expected_array_size, expected_array_size == 1 ? "" : "s",
               present_disks, present_disks == 1 ? "" : "s");
        first_issue = 0;
    }

    /* Add failed disk issues */
    for (int i = 0; i < 5; i++) {
        if (disks[i].is_present && disks[i].overall_status == DISK_STATUS_FAILED) {
            if (!first_issue) printf(",");
            printf("\n      \"Disk %d (%s): SMART health check failed\"",
                   i, disks[i].disk_name[0] ? disks[i].disk_name : "Unknown");
            first_issue = 0;
        }
    }

    if (!first_issue) {
        printf("\n    ");
    }
    printf("]\n");
    printf("  },\n");

    printf("  \"disks\": [\n");

    int first_disk = 1;
    for (int i = 0; i < 5; i++) {
        if (!disks[i].is_present) {
            continue;
        }

        if (!first_disk) {
            printf(",\n");
        }
        first_disk = 0;

        printf("    {\n");
        printf("      \"disk_number\": %d,\n", i);
        printf("      \"model\": \"%s\",\n",
               disks[i].disk_name[0] ? disks[i].disk_name : "Unknown");

        if (disks[i].serial_number[0] != '\0') {
            printf("      \"serial\": \"%s\",\n", disks[i].serial_number);
        }

        if (disks[i].firmware_rev[0] != '\0') {
            printf("      \"firmware\": \"%s\",\n", disks[i].firmware_rev);
        }

        if (disks[i].size_mb > 0) {
            printf("      \"size_mb\": %llu,\n", (unsigned long long)disks[i].size_mb);
        }

        {
            const char* s;
            switch (disks[i].overall_status) {
                case DISK_STATUS_PASSED: s = "healthy"; break;
                case DISK_STATUS_FAILED: s = "failed";  break;
                default:                 s = "error";   break;
            }
            printf("      \"overall_status\": \"%s\",\n", s);
        }

        int temp = get_temperature(&disks[i]);
        if (temp >= 0) {
            printf("      \"temperature_celsius\": %d,\n", temp);
        }

        uint64_t hours = get_power_on_hours(&disks[i]);
        if (hours > 0) {
            printf("      \"power_on_hours\": %llu,\n", (unsigned long long)hours);
        }

        printf("      \"attributes\": [\n");

        for (int j = 0; j < disks[i].num_attributes; j++) {
            const parsed_smart_attribute_t* attr = &disks[i].attributes[j];

            printf("        {\n");
            printf("          \"id\": %d,\n", attr->id);
            printf("          \"name\": \"%s\",\n", attr->name);
            printf("          \"value\": %d,\n", attr->current_value);
            printf("          \"worst\": %d,\n", attr->worst_value);
            printf("          \"thresh\": %d,\n", attr->threshold);
            printf("          \"raw\": %llu,\n", (unsigned long long)attr->raw_value);
            {
                const char* s;
                switch (attr->status) {
                    case ATTR_STATUS_PASSED:  s = "ok";      break;
                    case ATTR_STATUS_FAILED:  s = "failed";  break;
                    default:                  s = "unknown"; break;
                }
                printf("          \"status\": \"%s\",\n", s);
            }
            printf("          \"critical\": %s\n", attr->is_critical ? "true" : "false");
            printf("        }%s\n", (j < disks[i].num_attributes - 1) ? "," : "");
        }

        printf("      ]\n");
        printf("    }");
    }

    printf("\n  ]\n");
    printf("}\n");
}

void format_raw(const uint8_t* data, uint32_t len, const char* label) {
    if (label != NULL) {
        printf("%s:\n", label);
    }

    for (uint32_t i = 0; i < len; i++) {
        printf("0x%02x, ", data[i]);
        if ((i & 0x0f) == 0x0f) {
            /* Print ASCII representation */
            printf("   ");
            for (uint32_t j = i - 0x0f; j <= i; j++) {
                uint8_t c = data[j];
                if (c >= 0x20 && c < 0x7f) {
                    printf("%c", c);
                } else {
                    printf(".");
                }
            }
            printf("\n");
        }
    }

    if ((len & 0x0f) != 0) {
        printf("\n");
    }
}
