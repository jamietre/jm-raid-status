/*
 * output_formatter.h - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef OUTPUT_FORMATTER_H
#define OUTPUT_FORMATTER_H

#include "smart_parser.h"
#include <stdint.h>

/* Output format modes */
typedef enum {
    OUTPUT_MODE_SUMMARY = 0,
    OUTPUT_MODE_FULL = 1,
    OUTPUT_MODE_JSON = 2,
    OUTPUT_MODE_RAW = 3
} output_mode_t;

/**
 * Format and print summary view for all disks
 * Shows overall health status and key metrics
 *
 * @param device_path Device path (e.g., "/dev/sdc")
 * @param disks Array of disk SMART data
 * @param num_disks Number of disks in array
 * @param controller_model Controller model string (optional, can be NULL)
 */
void format_summary(const char* device_path, const disk_smart_data_t* disks, int num_disks, const char* controller_model);

/**
 * Format and print full SMART attribute table for one disk
 * Shows all SMART attributes in table format
 *
 * @param disk Disk SMART data
 */
void format_full_smart(const disk_smart_data_t* disk);

/**
 * Format and print JSON output for all disks
 * Machine-readable format for scripting
 *
 * @param device_path Device path (e.g., "/dev/sdc")
 * @param disks Array of disk SMART data
 * @param num_disks Number of disks in array
 */
void format_json(const char* device_path, const disk_smart_data_t* disks, int num_disks);

/**
 * Format and print raw hex dump (original behavior)
 * For debugging and backward compatibility
 *
 * @param data Raw data buffer
 * @param len Length of data
 * @param label Label for this data dump
 */
void format_raw(const uint8_t* data, uint32_t len, const char* label);

/**
 * Get string representation of disk health status
 *
 * @param status Disk health status
 * @return Status string ("PASSED", "FAILED", "ERROR")
 */
const char* disk_status_string(disk_health_status_t status);

/**
 * Get string representation of attribute health status
 *
 * @param status Attribute health status
 * @return Status string ("OK", "WARNING", "CRITICAL", "UNKNOWN")
 */
const char* attribute_status_string(attribute_health_status_t status);

#endif /* OUTPUT_FORMATTER_H */
