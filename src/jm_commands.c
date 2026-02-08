/*
 * jm_commands.c - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "jm_commands.h"
#include "jm_protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <asm/byteorder.h>

#define JM_RAID_SCRAMBLED_CMD (0x197b0322)

/* Command counter for scrambled commands */
static uint32_t cmd_counter = 1;

/* Global runtime context (singleton) */
static const jm_runtime_context_t* g_runtime_context = NULL;

void jm_set_context(const jm_runtime_context_t* ctx) {
    g_runtime_context = ctx;
}

const jm_runtime_context_t* jm_get_context(void) {
    return g_runtime_context;
}

/* Helper function to build and execute a scrambled command */
static int execute_probe_command(int fd, const uint8_t* probe_data, size_t probe_len,
                                 uint8_t* response, uint32_t sector) {
    uint8_t cmd_buf[512];
    uint32_t* cmd_buf32 = (uint32_t*)cmd_buf;
    uint32_t* resp_buf32 = (uint32_t*)response;

    /* Zero-fill command buffer */
    memset(cmd_buf, 0, 512);

    /* Add scrambled command header */
    cmd_buf32[0] = __cpu_to_le32(JM_RAID_SCRAMBLED_CMD);
    cmd_buf32[1] = __cpu_to_le32(cmd_counter++);

    /* Copy probe command data */
    memcpy(cmd_buf + 8, probe_data, probe_len);

    /* Execute command */
    int result = jm_execute_command(fd, cmd_buf32, resp_buf32, sector);

    /* Return success only if command succeeded */
    return (result == JM_SUCCESS) ? 0 : -1;
}

/* Helper: Swap bytes within 16-bit words for ATA strings */
static void ata_string_swap(char* dest, const uint8_t* src, int len) {
    for (int i = 0; i < len; i += 2) {
        dest[i] = src[i + 1];
        dest[i + 1] = src[i];
    }
    dest[len] = '\0';

    /* Trim trailing spaces */
    for (int i = len - 1; i >= 0 && dest[i] == ' '; i--) {
        dest[i] = '\0';
    }

    /* Trim leading spaces by shifting the string */
    int leading_spaces = 0;
    while (dest[leading_spaces] == ' ' && dest[leading_spaces] != '\0') {
        leading_spaces++;
    }
    if (leading_spaces > 0) {
        int new_len = len - leading_spaces;
        for (int i = 0; i <= new_len; i++) {  /* Include null terminator */
            dest[i] = dest[i + leading_spaces];
        }
    }
}

/* Helper: Check if IDENTIFY response looks like a real disk vs. garbage/empty slot */
static int validate_identify_response(const uint8_t* response) {
    /* Check model string (offset 0x10) - should be mostly printable ASCII */
    const uint8_t* model_bytes = response + 0x10;
    int printable_count = 0;
    int total_non_space = 0;

    for (int i = 0; i < 32; i++) {
        uint8_t c = model_bytes[i];
        if (c >= 0x20 && c < 0x7f) {
            printable_count++;
            if (c != ' ') total_non_space++;
        }
    }

    /* Real disks have printable model strings with actual content
     * Empty slots return all zeros (0 printable chars)
     * Require at least 8 printable chars and at least 5 non-space chars
     * to distinguish real disks from garbage/empty slots */
    if (printable_count < 8 || total_non_space < 5) {
        return 0;  // Not a valid disk response
    }

    /* Check if response is all zeros or all 0xFF (common for empty slots) */
    int all_zero = 1;
    int all_ff = 1;
    for (int i = 0; i < 64; i++) {
        if (response[i] != 0x00) all_zero = 0;
        if (response[i] != 0xFF) all_ff = 0;
    }

    if (all_zero || all_ff) {
        return 0;  // Empty slot
    }

    return 1;  // Looks like a valid disk
}

int jm_get_disk_identify(int fd, int disk_num, char* model, char* serial, char* firmware, uint64_t* size_mb, uint8_t* disk_bitmask) {
    if (disk_num < 0 || disk_num > 4) {
        return -1;
    }

    const jm_runtime_context_t* ctx = jm_get_context();
    if (ctx == NULL) {
        fprintf(stderr, "Error: Runtime context not initialized\n");
        return -1;
    }

    /* Build IDENTIFY DEVICE command for specified disk (probe11-15) */
    uint8_t probe_cmd[] = {
        0x00, 0x02, 0x02, 0xff,
        (uint8_t)disk_num,  // Disk number
        0x00, 0x00, 0x00, 0x00,
        (uint8_t)disk_num   // Disk number again
    };

    uint8_t response[512];

    /* Execute command - CRC failures indicate communication errors */
    if (execute_probe_command(fd, probe_cmd, sizeof(probe_cmd), response, ctx->sector) != 0) {
        /* CRC error or communication failure - this is a real error */
        return -1;
    }

    /* Dump raw response if debug mode enabled */
    if (ctx->dump_raw) {
        fprintf(stderr, "\n=== IDENTIFY DISK %d RESPONSE (512 bytes) ===\n", disk_num);
        for (int i = 0; i < 512; i += 16) {
            fprintf(stderr, "%04x: ", i);
            for (int j = 0; j < 16 && i + j < 512; j++) {
                fprintf(stderr, "%02x ", response[i + j]);
            }
            fprintf(stderr, " |");
            for (int j = 0; j < 16 && i + j < 512; j++) {
                uint8_t c = response[i + j];
                fprintf(stderr, "%c", (c >= 32 && c < 127) ? c : '.');
            }
            fprintf(stderr, "|\n");
        }
        fprintf(stderr, "\n");
    }

    /* Read disk presence bitmask at offset 0x1F0
     * This is a bitmask where each bit represents a disk slot:
     *   Bit 0 = disk 0, Bit 1 = disk 1, Bit 2 = disk 2, Bit 3 = disk 3
     * Examples:
     *   0x0F (1111b) = All 4 disks present
     *   0x07 (0111b) = Disks 0,1,2 present (disk 3 missing)
     *   0x03 (0011b) = Disks 0,1 present
     * This flag appears in ALL disk responses (even empty slots) */
    uint8_t bitmask_value = response[0x1F0];

    /* Return bitmask to caller if requested */
    if (disk_bitmask != NULL) {
        *disk_bitmask = bitmask_value;
    }

    /* Command succeeded with valid CRC, now check if response looks like a real disk */
    if (!validate_identify_response(response)) {
        /* Valid communication but no disk present (empty slot) */
        return -2;  /* Special code: slot empty, not an error */
    }

    /* JMicron IDENTIFY DEVICE response format:
     * Offset 0x00-0x0F: JMicron header
     * Offset 0x10-0x2F: Model number (32 bytes, byte-swapped)
     * Offset 0x30-0x3F: Serial number (16 bytes, byte-swapped)
     * Offset 0x50-0x57: Firmware revision (8 bytes, byte-swapped)
     * Offset 0xC8-0xCF: 48-bit LBA sector count (8 bytes, little-endian) */

    if (model != NULL) {
        ata_string_swap(model, response + 0x10, 32);
    }
    if (serial != NULL) {
        ata_string_swap(serial, response + 0x30, 16);
    }
    if (firmware != NULL) {
        ata_string_swap(firmware, response + 0x50, 8);
    }
    if (size_mb != NULL) {
        /* Parse disk capacity from JMicron IDENTIFY response
         * Found at offset 0x4A-0x4F: 48-bit sector count (little-endian) */
        uint64_t sectors = 0;
        for (int i = 0; i < 6; i++) {
            sectors |= ((uint64_t)response[0x4A + i]) << (i * 8);
        }

        /* Sanity check: should be in range 1TB to 25TB */
        if (sectors >= 2000000000ULL && sectors <= 50000000000ULL) {
            *size_mb = (sectors * 512) / (1024 * 1024);
        } else {
            *size_mb = 0;
        }
    }

    return 0;  /* Success: real disk with valid data */
}

int jm_get_disk_names(int fd, char disk_names[5][64]) {
    /* Use IDENTIFY DEVICE to get model names */
    for (int i = 0; i < 5; i++) {
        if (jm_get_disk_identify(fd, i, disk_names[i], NULL, NULL, NULL, NULL) != 0) {
            memset(disk_names[i], 0, 64);
        }
    }
    return 0;
}

int jm_smart_read_values(int fd, int disk_num, smart_values_page_t* values) {
    if (disk_num < 0 || disk_num > 4 || values == NULL) {
        return -1;
    }

    const jm_runtime_context_t* ctx = jm_get_context();
    if (ctx == NULL) {
        fprintf(stderr, "Error: Runtime context not initialized\n");
        return -1;
    }

    /* Build SMART READ ATTRIBUTE VALUE command for specified disk */
    uint8_t probe_cmd[] = {
        0x00, 0x02, 0x03, 0xff,
        (uint8_t)disk_num,  // Disk number
        0x02, 0x00, 0xe0, 0x00, 0x00,
        0xd0,  // SMART READ ATTRIBUTE VALUE
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x4f, 0x00, 0xc2, 0x00, 0xa0, 0x00, 0xb0, 0x00
    };

    uint8_t response[512];

    if (execute_probe_command(fd, probe_cmd, sizeof(probe_cmd), response, ctx->sector) != 0) {
        return -1;
    }

    /* Dump raw response if debug mode enabled */
    if (ctx->dump_raw) {
        fprintf(stderr, "\n=== SMART VALUES DISK %d RESPONSE (512 bytes) ===\n", disk_num);
        fprintf(stderr, "First 32 bytes are JMicron header/echo:\n");
        for (int i = 0; i < 32; i += 16) {
            fprintf(stderr, "%04x: ", i);
            for (int j = 0; j < 16 && i + j < 32; j++) {
                fprintf(stderr, "%02x ", response[i + j]);
            }
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "Remaining bytes are SMART data:\n");
        for (int i = 32; i < 512; i += 16) {
            fprintf(stderr, "%04x: ", i);
            for (int j = 0; j < 16 && i + j < 512; j++) {
                fprintf(stderr, "%02x ", response[i + j]);
            }
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "\n");
    }

    /* The JMicron response has the actual SMART data starting at offset 0x20 (32 bytes)
     * The first 32 bytes contain the JMicron command header/echo */
    return smart_parse_values(response + 0x20, values);
}

int jm_smart_read_thresholds(int fd, int disk_num, smart_thresholds_page_t* thresholds) {
    if (disk_num < 0 || disk_num > 4 || thresholds == NULL) {
        return -1;
    }

    const jm_runtime_context_t* ctx = jm_get_context();
    if (ctx == NULL) {
        fprintf(stderr, "Error: Runtime context not initialized\n");
        return -1;
    }

    /* Build SMART READ ATTRIBUTE THRESHOLDS command for specified disk */
    uint8_t probe_cmd[] = {
        0x00, 0x02, 0x03, 0xff,
        (uint8_t)disk_num,  // Disk number
        0x02, 0x00, 0xe0, 0x00, 0x00,
        0xd1,  // SMART READ ATTRIBUTE THRESHOLDS
        0x00, 0x00, 0x00, 0x00, 0x00,
        0x4f, 0x00, 0xc2, 0x00, 0xa0, 0x00, 0xb0, 0x00
    };

    uint8_t response[512];

    if (execute_probe_command(fd, probe_cmd, sizeof(probe_cmd), response, ctx->sector) != 0) {
        return -1;
    }

    /* The JMicron response has the actual SMART data starting at offset 0x20 (32 bytes)
     * The first 32 bytes contain the JMicron command header/echo */
    return smart_parse_thresholds(response + 0x20, thresholds);
}

int jm_get_disk_smart_data(int fd, int disk_num, const char* disk_name,
                            disk_smart_data_t* data) {
    smart_values_page_t values;
    smart_thresholds_page_t thresholds;

    if (data == NULL) {
        return -1;
    }

    /* Initialize basic disk info */
    memset(data, 0, sizeof(disk_smart_data_t));
    data->disk_number = disk_num;
    data->is_present = 1;
    if (disk_name != NULL) {
        strncpy(data->disk_name, disk_name, sizeof(data->disk_name) - 1);
        data->disk_name[sizeof(data->disk_name) - 1] = '\0';
    }

    /* Read SMART values (optional - disk still shown if unavailable) */
    if (jm_smart_read_values(fd, disk_num, &values) != 0) {
        /* SMART VALUES unavailable - show disk with warning */
        data->overall_status = DISK_STATUS_ERROR;
        fprintf(stderr, "Warning: SMART data unavailable for disk %d (%s)\n",
                disk_num, disk_name ? disk_name : "Unknown");
        return 0;  /* Success - disk exists but no SMART data */
    }

    /* Read SMART thresholds (optional - will use defaults if unavailable) */
    if (jm_smart_read_thresholds(fd, disk_num, &thresholds) != 0) {
        /* Thresholds unavailable - zero them out and use default checks instead */
        memset(&thresholds, 0, sizeof(thresholds));
        fprintf(stderr, "Warning: SMART thresholds unavailable for disk %d, using default checks\n", disk_num);
    }

    /* Combine and assess health */
    return smart_combine_data(disk_num, disk_name, &values, &thresholds, data);
}

int jm_get_all_disks_smart_data(int fd, disk_smart_data_t data[5], int* num_disks, int* is_degraded, int* present_disks) {
    int disks_found = 0;
    int degraded = 0;
    uint8_t disk_bitmask = 0;
    int bitmask_captured = 0;

    if (data == NULL || num_disks == NULL) {
        return -1;
    }

    const jm_runtime_context_t* ctx = jm_get_context();
    if (ctx == NULL) {
        fprintf(stderr, "Error: Runtime context not initialized\n");
        return -1;
    }

    if (is_degraded != NULL) {
        *is_degraded = 0;
    }

    /* Query each possible disk (0-4) */
    for (int i = 0; i < 5; i++) {
        char model[41] = {0};
        char serial[21] = {0};
        char firmware[9] = {0};
        uint64_t size_mb = 0;
        uint8_t bitmask_temp = 0;

        if (ctx->verbose) {
            fprintf(stderr, "  Probing disk slot %d...\n", i);
        }

        /* Initialize data for this slot */
        memset(&data[i], 0, sizeof(disk_smart_data_t));
        data[i].disk_number = i;
        data[i].is_present = 0;

        /* First try to IDENTIFY the disk
         * Return codes:
         *   0 = disk present and identified successfully
         *  -1 = communication error (CRC failure, etc.) - will have printed warning
         *  -2 = no disk in slot (empty, but communication OK) */
        int identify_result = jm_get_disk_identify(fd, i, model, serial, firmware, &size_mb, &bitmask_temp);

        /* Capture disk bitmask from first successful response (even if slot is empty)
         * The bitmask is the same in all responses, so we only need it once */
        if (!bitmask_captured && (identify_result == 0 || identify_result == -2)) {
            disk_bitmask = bitmask_temp;
            bitmask_captured = 1;
        }

        if (identify_result == -2) {
            /* Empty slot - not an error, just continue */
            if (ctx->verbose) {
                fprintf(stderr, "    Slot %d: Empty (no disk present)\n", i);
            }
            continue;
        } else if (identify_result != 0) {
            /* Communication error - warning already printed, skip this disk */
            if (ctx->verbose) {
                fprintf(stderr, "    Slot %d: Communication error\n", i);
            }
            continue;
        }

        if (ctx->verbose) {
            fprintf(stderr, "    Slot %d: Found disk - %s\n", i, model);
        }

        /* Get SMART data */
        if (jm_get_disk_smart_data(fd, i, model, &data[i]) == 0) {
            /* Disk exists and SMART data retrieved successfully - store disk info
             * NOTE: Must do this AFTER jm_get_disk_smart_data because smart_combine_data
             * clears the structure with memset() */
            strncpy(data[i].serial_number, serial, sizeof(data[i].serial_number) - 1);
            data[i].serial_number[sizeof(data[i].serial_number) - 1] = '\0';
            strncpy(data[i].firmware_rev, firmware, sizeof(data[i].firmware_rev) - 1);
            data[i].firmware_rev[sizeof(data[i].firmware_rev) - 1] = '\0';
            data[i].size_mb = size_mb;
            disks_found++;
        }
    }

    *num_disks = disks_found;

    /* Check for degraded RAID using the disk presence bitmask from 0x1F0
     * The controller reports which disks are present via a bitmask
     * If expected_array_size is specified, compare actual vs expected */
    if (ctx->expected_array_size > 0 && bitmask_captured) {
        /* Count the number of present disks using popcount */
        int disks_from_bitmask = 0;
        for (int i = 0; i < 8; i++) {
            if (disk_bitmask & (1 << i)) {
                disks_from_bitmask++;
            }
        }

        /* Return bitmask count to caller if requested */
        if (present_disks != NULL) {
            *present_disks = disks_from_bitmask;
        }

        /* Compare to expected array size */
        if (disks_from_bitmask < ctx->expected_array_size) {
            degraded = 1;
            if (ctx->verbose) {
                fprintf(stderr, "\n*** DEGRADED RAID DETECTED (bitmask 0x%02x) ***\n", disk_bitmask);
                fprintf(stderr, "    Expected %d disk%s, found %d disk%s present\n",
                        ctx->expected_array_size, ctx->expected_array_size == 1 ? "" : "s",
                        disks_from_bitmask, disks_from_bitmask == 1 ? "" : "s");
                fprintf(stderr, "    RAID array is operating in degraded mode\n");
                fprintf(stderr, "    One or more disks have failed or been removed\n");
                fprintf(stderr, "    CRITICAL: Array has REDUCED or NO redundancy!\n\n");
            }
        } else if (disks_from_bitmask > ctx->expected_array_size) {
            if (ctx->verbose) {
                fprintf(stderr, "\n*** WARNING: MORE DISKS THAN EXPECTED (bitmask 0x%02x) ***\n", disk_bitmask);
                fprintf(stderr, "    Expected %d disk%s, found %d disk%s present\n",
                        ctx->expected_array_size, ctx->expected_array_size == 1 ? "" : "s",
                        disks_from_bitmask, disks_from_bitmask == 1 ? "" : "s");
                fprintf(stderr, "    This may indicate:\n");
                fprintf(stderr, "    - Incorrect --array-size specified\n");
                fprintf(stderr, "    - Extra disk added to array\n");
                fprintf(stderr, "    - Array configuration changed\n\n");
            }
        }
    }

    if (is_degraded != NULL) {
        *is_degraded = degraded;
    }

    return (disks_found > 0) ? 0 : -1;
}
