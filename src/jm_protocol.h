/*
 * jm_protocol.h - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef JM_PROTOCOL_H
#define JM_PROTOCOL_H

#include <stdint.h>

#define JM_SECTORSIZE 512

/* Error codes */
typedef enum {
    JM_SUCCESS = 0,
    JM_ERROR_DEVICE_OPEN = 1,
    JM_ERROR_NOT_SG_DEVICE = 2,
    JM_ERROR_IOCTL_FAILED = 3,
    JM_ERROR_CRC_MISMATCH = 4,
    JM_ERROR_INVALID_ARGS = 5
} jm_error_code_t;

/**
 * Initialize JMicron device for communication
 * Opens device, verifies it's an SG device, and backs up the working sector
 *
 * @param device_path Path to SCSI generic device (e.g., "/dev/sdc")
 * @param fd_out Output file descriptor for the opened device
 * @param backup_sector Buffer to store backup of sector (must be 512 bytes)
 * @param sector Sector number to use for communication (default: 0x21)
 * @return JM_SUCCESS on success, error code on failure
 */
int jm_init_device(const char* device_path, int* fd_out, uint8_t* backup_sector, uint32_t sector);

/**
 * Clean up and restore original sector data
 *
 * @param fd File descriptor from jm_init_device
 * @param backup_sector Backup sector data to restore
 * @param sector Sector number that was used
 * @return JM_SUCCESS on success, error code on failure
 */
int jm_cleanup_device(int fd, const uint8_t* backup_sector, uint32_t sector);

/**
 * Send wakeup sequence to JMicron controller
 * This must be called after jm_init_device before any other commands
 *
 * @param fd File descriptor from jm_init_device
 * @param sector Sector number to use for communication
 * @return JM_SUCCESS on success, error code on failure
 */
int jm_send_wakeup(int fd, uint32_t sector);

/**
 * Execute a JMicron scrambled command
 * Handles CRC calculation, XOR scrambling, and response validation
 *
 * @param fd File descriptor from jm_init_device
 * @param cmd_buf Command buffer (128 uint32_t = 512 bytes)
 * @param resp_buf Response buffer (128 uint32_t = 512 bytes)
 * @param sector Sector number to use for communication
 * @return JM_SUCCESS on success, JM_ERROR_CRC_MISMATCH if response CRC invalid
 */
int jm_execute_command(int fd, uint32_t* cmd_buf, uint32_t* resp_buf, uint32_t sector);

/**
 * Get human-readable error message for error code
 *
 * @param error_code Error code from jm_* functions
 * @return Error message string
 */
const char* jm_error_string(jm_error_code_t error_code);

#endif /* JM_PROTOCOL_H */
