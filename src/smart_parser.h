/*
 * smart_parser.h - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef SMART_PARSER_H
#define SMART_PARSER_H

#include <stdint.h>

#define MAX_SMART_ATTRIBUTES 30

/* SMART attribute entry as stored in disk response (12 bytes, ATA standard) */
typedef struct {
    uint8_t id;
    uint16_t flags;
    uint8_t current_value;
    uint8_t worst_value;
    uint8_t raw_value[6];
    uint8_t reserved;
} __attribute__((packed)) smart_attribute_t;

/* SMART threshold entry (12 bytes) */
typedef struct {
    uint8_t id;
    uint8_t threshold;
    uint8_t reserved[10];
} __attribute__((packed)) smart_threshold_t;

/* Health status for individual attributes */
typedef enum {
    ATTR_STATUS_PASSED = 0,
    ATTR_STATUS_FAILED = 1,
    ATTR_STATUS_UNKNOWN = 2
} attribute_health_status_t;

/* Overall disk health status */
typedef enum {
    DISK_STATUS_PASSED = 0,
    DISK_STATUS_FAILED = 1,
    DISK_STATUS_ERROR = 2
} disk_health_status_t;

/* Parsed SMART attribute with health assessment */
typedef struct {
    uint8_t id;
    const char* name;
    uint8_t current_value;
    uint8_t worst_value;
    uint8_t threshold;
    uint64_t raw_value;
    attribute_health_status_t status;
    int is_critical;        // 1 if this attribute is critical for disk health
} parsed_smart_attribute_t;

/* Complete SMART data for one disk */
typedef struct {
    int disk_number;
    char disk_name[64];
    char serial_number[21];
    char firmware_rev[9];
    uint64_t size_mb;
    disk_health_status_t overall_status;
    parsed_smart_attribute_t attributes[MAX_SMART_ATTRIBUTES];
    int num_attributes;
    int is_present;         // 0 if disk not present in array
} disk_smart_data_t;

/* SMART values page structure (512 bytes) */
typedef struct {
    uint16_t revision;
    smart_attribute_t attributes[30];
    uint8_t offline_data_collection_status;
    uint8_t vendor_specific1;
    uint16_t offline_data_collection_time;
    uint8_t vendor_specific2;
    uint8_t offline_data_collection_capability;
    uint16_t smart_capability;
    uint8_t error_logging_capability;
    uint8_t vendor_specific3;
    uint8_t short_test_time;
    uint8_t extended_test_time;
    uint8_t conveyance_test_time;
    uint8_t reserved[11];
    uint8_t vendor_specific[125];
} __attribute__((packed)) smart_values_page_t;

/* SMART thresholds page structure (512 bytes) */
typedef struct {
    uint16_t revision;
    smart_threshold_t thresholds[30];
    uint8_t reserved[149];
} __attribute__((packed)) smart_thresholds_page_t;

/* Function declarations */

/**
 * Parse SMART attribute values from raw 512-byte response
 * @param raw_data Raw 512-byte buffer from SMART READ ATTRIBUTE VALUE (0xD0)
 * @param values Output parsed values structure
 * @return 0 on success, -1 on error
 */
int smart_parse_values(const uint8_t* raw_data, smart_values_page_t* values);

/**
 * Parse SMART attribute thresholds from raw 512-byte response
 * @param raw_data Raw 512-byte buffer from SMART READ ATTRIBUTE THRESHOLDS (0xD1)
 * @param thresholds Output parsed thresholds structure
 * @return 0 on success, -1 on error
 */
int smart_parse_thresholds(const uint8_t* raw_data, smart_thresholds_page_t* thresholds);

/**
 * Combine values and thresholds to create parsed SMART data
 * @param disk_num Disk number (0-4)
 * @param disk_name Disk model name
 * @param values Parsed SMART values
 * @param thresholds Parsed SMART thresholds
 * @param data Output combined disk data with health assessment
 * @return 0 on success, -1 on error
 */
int smart_combine_data(int disk_num, const char* disk_name,
                       const smart_values_page_t* values,
                       const smart_thresholds_page_t* thresholds,
                       disk_smart_data_t* data);

/**
 * Assess health of a single attribute
 * @param attr Parsed attribute with values and threshold
 * @return Health status (PASSED/FAILED/UNKNOWN)
 */
attribute_health_status_t assess_attribute_health(const parsed_smart_attribute_t* attr);

/**
 * Assess overall disk health based on all attributes
 * @param data Disk SMART data with parsed attributes
 * @return Overall disk health status
 */
disk_health_status_t assess_overall_health(disk_smart_data_t* data);

/**
 * Extract 48-bit raw value from SMART attribute raw_value field
 * @param raw_value 6-byte raw value array
 * @return 64-bit integer value
 */
uint64_t smart_raw_value_to_uint64(const uint8_t* raw_value);

#endif /* SMART_PARSER_H */
