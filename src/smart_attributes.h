/*
 * smart_attributes.h - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef SMART_ATTRIBUTES_H
#define SMART_ATTRIBUTES_H

#include <stdint.h>

/* SMART attribute definition */
typedef struct {
    uint8_t id;
    const char* name;
    const char* description;
    int is_critical;        // 1 if failure indicates imminent disk failure
} smart_attribute_def_t;

/**
 * Get attribute definition by ID
 * @param id SMART attribute ID (e.g., 0x05 for Reallocated Sector Count)
 * @return Pointer to attribute definition, or NULL if unknown
 */
const smart_attribute_def_t* get_attribute_definition(uint8_t id);

/**
 * Check if attribute ID is critical for disk health
 * @param id SMART attribute ID
 * @return 1 if critical, 0 if not critical
 */
int is_critical_attribute(uint8_t id);

#endif /* SMART_ATTRIBUTES_H */
