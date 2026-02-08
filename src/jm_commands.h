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
 * Read disk identify information from RAID controller
 * Executes ATA IDENTIFY DEVICE command (probe11-15) to get disk info
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_num Disk number (0-4)
 * @param model Output buffer for model name (40 bytes minimum)
 * @param serial Output buffer for serial number (20 bytes minimum)
 * @param firmware Output buffer for firmware revision (8 bytes minimum)
 * @param size_mb Output pointer for disk size in MB (can be NULL)
 * @param sector Sector number for communication
 * @param dump_raw If non-zero, dump raw protocol data to stderr
 * @param disk_bitmask Output pointer for disk presence bitmask from 0x1F0 (can be NULL)
 * @return 0 on success, -1 on error, -2 on empty slot
 */
int jm_get_disk_identify(int fd, int disk_num, char* model, char* serial, char* firmware, uint64_t* size_mb, uint32_t sector, int dump_raw, uint8_t* disk_bitmask);

/**
 * Read disk names from RAID controller (deprecated - use jm_get_disk_identify)
 * Executes probe9 command to get disk model names
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_names Output array of 5 disk name strings (64 bytes each)
 * @param sector Sector number for communication
 * @return 0 on success, -1 on error
 */
int jm_get_disk_names(int fd, char disk_names[5][64], uint32_t sector);

/**
 * Read SMART attribute values for a disk
 * Executes ATA SMART READ ATTRIBUTE VALUE (0xD0) command
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_num Disk number (0-4)
 * @param values Output SMART values structure
 * @param sector Sector number for communication
 * @param dump_raw If non-zero, dump raw protocol data to stderr
 * @return 0 on success, -1 on error
 */
int jm_smart_read_values(int fd, int disk_num, smart_values_page_t* values, uint32_t sector, int dump_raw);

/**
 * Read SMART attribute thresholds for a disk
 * Executes ATA SMART READ ATTRIBUTE THRESHOLDS (0xD1) command
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_num Disk number (0-4)
 * @param thresholds Output SMART thresholds structure
 * @param sector Sector number for communication
 * @return 0 on success, -1 on error
 */
int jm_smart_read_thresholds(int fd, int disk_num, smart_thresholds_page_t* thresholds, uint32_t sector);

/**
 * Get complete SMART data for a disk (values + thresholds + health assessment)
 *
 * @param fd File descriptor from jm_init_device
 * @param disk_num Disk number (0-4)
 * @param disk_name Disk model name (can be NULL)
 * @param data Output complete SMART data with health assessment
 * @param sector Sector number for communication
 * @param dump_raw If non-zero, dump raw protocol data to stderr
 * @return 0 on success, -1 on error
 */
int jm_get_disk_smart_data(int fd, int disk_num, const char* disk_name,
                            disk_smart_data_t* data, uint32_t sector, int dump_raw);

/**
 * Get SMART data for all disks in the array
 *
 * @param fd File descriptor from jm_init_device
 * @param data Output array of SMART data (must have space for 5 disks)
 * @param num_disks Output number of disks found
 * @param sector Sector number for communication
 * @param is_degraded Optional output: set to 1 if degraded RAID detected, 0 otherwise (can be NULL)
 * @param dump_raw If non-zero, dump raw protocol data to stderr
 * @param expected_array_size Expected number of disks (0 = auto-detect, no validation)
 * @return 0 on success, -1 on error
 */
int jm_get_all_disks_smart_data(int fd, disk_smart_data_t data[5], int* num_disks, uint32_t sector, int* is_degraded, int dump_raw, int expected_array_size);

#endif /* JM_COMMANDS_H */
