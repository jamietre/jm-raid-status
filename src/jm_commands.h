/*
 * jm_commands.h - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef JM_COMMANDS_H
#define JM_COMMANDS_H

#include "smart_parser.h"
#include <stdint.h>

/**
 * Global runtime context for command execution
 * Set once at program start, accessed globally
 */
typedef struct {
    int verbose;              /* Verbose output */
    int dump_raw;            /* Dump raw protocol data */
    uint32_t sector;         /* Sector number for communication */
    int expected_array_size; /* Expected number of disks (0 = not specified) */
} jm_runtime_context_t;

/**
 * Set global runtime context (singleton)
 * Call once at program startup
 */
void jm_set_context(const jm_runtime_context_t* ctx);

/**
 * Get global runtime context
 * @return Pointer to current context, or NULL if not set
 */
const jm_runtime_context_t* jm_get_context(void);

/**
 * Read disk identify information from RAID controller
 * Executes ATA IDENTIFY DEVICE command (probe11-15) to get disk info
 * Uses global runtime context for sector and dump_raw settings
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_num Disk number (0-4)
 * @param model Output buffer for model name (40 bytes minimum)
 * @param serial Output buffer for serial number (20 bytes minimum)
 * @param firmware Output buffer for firmware revision (8 bytes minimum)
 * @param size_mb Output pointer for disk size in MB (can be NULL)
 * @param disk_bitmask Output pointer for disk presence bitmask from 0x1F0 (can be NULL)
 * @return 0 on success, -1 on error, -2 on empty slot
 */
int jm_get_disk_identify(int fd, int disk_num, char* model, char* serial, char* firmware, uint64_t* size_mb, uint8_t* disk_bitmask);

/**
 * Read disk names from RAID controller (deprecated - use jm_get_disk_identify)
 * Executes probe9 command to get disk model names
 * Uses global runtime context for sector setting
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_names Output array of 5 disk name strings (64 bytes each)
 * @return 0 on success, -1 on error
 */
int jm_get_disk_names(int fd, char disk_names[5][64]);

/**
 * Read SMART attribute values for a disk
 * Executes ATA SMART READ ATTRIBUTE VALUE (0xD0) command
 * Uses global runtime context for sector and dump_raw settings
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_num Disk number (0-4)
 * @param values Output SMART values structure
 * @return 0 on success, -1 on error
 */
int jm_smart_read_values(int fd, int disk_num, smart_values_page_t* values);

/**
 * Read SMART attribute thresholds for a disk
 * Executes ATA SMART READ ATTRIBUTE THRESHOLDS (0xD1) command
 * Uses global runtime context for sector setting
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_num Disk number (0-4)
 * @param thresholds Output SMART thresholds structure
 * @return 0 on success, -1 on error
 */
int jm_smart_read_thresholds(int fd, int disk_num, smart_thresholds_page_t* thresholds);

/**
 * Get complete SMART data for a disk (values + thresholds + health assessment)
 * Uses global runtime context for sector and dump_raw settings
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_num Disk number (0-4)
 * @param disk_name Disk model name (can be NULL)
 * @param data Output complete SMART data with health assessment
 * @return 0 on success, -1 on error
 */
int jm_get_disk_smart_data(int fd, int disk_num, const char* disk_name,
                            disk_smart_data_t* data);

/**
 * Get SMART data for all disks in the array
 * Uses global runtime context for sector, dump_raw, verbose, and expected_array_size settings
 *
 * @param fd File descriptor from jm_init_device
 * @param data Output array of SMART data (must have space for 5 disks)
 * @param num_disks Output number of disks found
 * @param is_degraded Optional output: set to 1 if degraded RAID detected, 0 otherwise (can be NULL)
 * @param present_disks Optional output: number of disks reported by controller bitmask (can be NULL)
 * @return 0 on success, -1 on error
 */
int jm_get_all_disks_smart_data(int fd, disk_smart_data_t data[5], int* num_disks, int* is_degraded, int* present_disks);

#endif /* JM_COMMANDS_H */
