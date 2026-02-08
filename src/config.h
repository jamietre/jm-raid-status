/**
 * config.h - Configuration file handling for SMART thresholds
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* Attribute-specific thresholds */
typedef struct {
    uint8_t id;              /* SMART attribute ID (e.g., 0x05) */
    int has_raw_critical;    /* Whether raw_critical is set */
    uint64_t raw_critical;   /* Fail if raw value exceeds this */
} attribute_threshold_t;

/* Configuration structure */
typedef struct {
    /* Temperature thresholds */
    int has_temp_critical;
    uint8_t temp_critical;

    /* Attribute-specific thresholds */
    attribute_threshold_t* attributes;
    int num_attributes;

    /* Whether to use manufacturer thresholds as fallback */
    int use_manufacturer_thresholds;
} smart_config_t;

/**
 * Load configuration from JSON file
 * Returns 0 on success, -1 on error
 */
int config_load(const char* path, smart_config_t* config);

/**
 * Free configuration resources
 */
void config_free(smart_config_t* config);

/**
 * Write default configuration to file
 * Returns 0 on success, -1 on error
 */
int config_write_default(const char* path);

/**
 * Initialize default configuration (in-memory)
 */
void config_init_default(smart_config_t* config);

#endif /* CONFIG_H */
