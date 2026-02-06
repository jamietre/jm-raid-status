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
        case DISK_STATUS_GOOD:     return "GOOD";
        case DISK_STATUS_WARNING:  return "WARNING";
        case DISK_STATUS_CRITICAL: return "CRITICAL";
        case DISK_STATUS_ERROR:    return "ERROR";
        default:                   return "UNKNOWN";
    }
}

const char* attribute_status_string(attribute_health_status_t status) {
    switch (status) {
        case ATTR_STATUS_GOOD:     return "OK";
        case ATTR_STATUS_WARNING:  return "WARNING";
        case ATTR_STATUS_CRITICAL: return "CRITICAL";
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

    int critical_count = 0;
    int warning_count = 0;

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
            if (disks[i].overall_status == DISK_STATUS_WARNING) {
                printf("  Warning: Disk showing signs of wear\n");
            } else if (disks[i].overall_status == DISK_STATUS_CRITICAL) {
                printf("  CRITICAL: Disk may be failing - backup data immediately!\n");
            }
        }

        printf("\n");

        /* Count for overall status */
        if (disks[i].overall_status == DISK_STATUS_CRITICAL) {
            critical_count++;
        } else if (disks[i].overall_status == DISK_STATUS_WARNING) {
            warning_count++;
        }
    }

    /* Overall RAID health */
    printf("Overall RAID Health: ");
    if (critical_count > 0) {
        printf("CRITICAL - Check disk(s) immediately!\n");
    } else if (warning_count > 0) {
        printf("WARNING - Monitor disk(s) closely\n");
    } else {
        printf("GOOD - All disks healthy\n");
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
            printf(" [CRITICAL]");
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
                printf("  OK: No reallocated sectors\n");
            } else {
                printf("  WARNING: %llu reallocated sectors\n", (unsigned long long)attr->raw_value);
                has_issues = 1;
            }
        } else if (attr->id == 0xC5) {  // Pending sectors
            if (attr->raw_value == 0) {
                printf("  OK: No pending sectors\n");
            } else {
                printf("  WARNING: %llu pending sectors\n", (unsigned long long)attr->raw_value);
                has_issues = 1;
            }
        } else if (attr->id == 0xC6) {  // Uncorrectable sectors
            if (attr->raw_value == 0) {
                printf("  OK: No uncorrectable sectors\n");
            } else {
                printf("  CRITICAL: %llu uncorrectable sectors\n", (unsigned long long)attr->raw_value);
                has_issues = 1;
            }
        } else if (attr->id == 0xC2) {  // Temperature
            int temp = (int)(attr->raw_value & 0xFF);
            if (temp < 50) {
                printf("  OK: Temperature within normal range (%d°C)\n", temp);
            } else if (temp < 60) {
                printf("  WARNING: Temperature elevated (%d°C)\n", temp);
                has_issues = 1;
            } else {
                printf("  CRITICAL: Temperature too high (%d°C)\n", temp);
                has_issues = 1;
            }
        }
    }

    if (!has_issues) {
        printf("  All critical parameters within acceptable range\n");
    }

    printf("\n");
}

void format_json(const char* device_path, const disk_smart_data_t* disks, int num_disks) {
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", tm_info);

    (void)num_disks;  /* Unused but kept for API consistency */

    printf("{\n");
    printf("  \"version\": \"1.0\",\n");
    printf("  \"device\": \"%s\",\n", device_path);
    printf("  \"timestamp\": \"%s\",\n", timestamp);
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
        printf("      \"name\": \"%s\",\n",
               disks[i].disk_name[0] ? disks[i].disk_name : "Unknown");

        if (disks[i].serial_number[0] != '\0') {
            printf("      \"serial_number\": \"%s\",\n", disks[i].serial_number);
        }

        if (disks[i].firmware_rev[0] != '\0') {
            printf("      \"firmware_revision\": \"%s\",\n", disks[i].firmware_rev);
        }

        if (disks[i].size_mb > 0) {
            printf("      \"size_mb\": %llu,\n", (unsigned long long)disks[i].size_mb);
        }

        printf("      \"status\": \"%s\",\n", disk_status_string(disks[i].overall_status));

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
            printf("          \"current\": %d,\n", attr->current_value);
            printf("          \"worst\": %d,\n", attr->worst_value);
            printf("          \"threshold\": %d,\n", attr->threshold);
            printf("          \"raw_value\": %llu,\n", (unsigned long long)attr->raw_value);
            printf("          \"status\": \"%s\",\n", attribute_status_string(attr->status));
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
