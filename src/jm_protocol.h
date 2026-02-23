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
 * Opens device, verifies it's an SG device, and sets up the SG_IO header.
 *
 * The caller is responsible for verifying the sector is safe to use before
 * calling this function (use jm_read_sector_block for the block device check).
 *
 * @param device_path Path to SCSI generic device (e.g., "/dev/sdc")
 * @param fd_out Output file descriptor for the opened device
 * @return JM_SUCCESS on success, error code on failure
 */
int jm_init_device(const char* device_path, int* fd_out);

/**
 * Clean up and restore sector to zeros
 * Can be called multiple times safely (idempotent)
 *
 * @param fd File descriptor from jm_init_device
 * @param sector Sector number that was used
 * @return JM_SUCCESS on success, error code on failure
 */
int jm_cleanup_device(int fd, uint32_t sector);

/**
 * Setup signal handlers to ensure sector cleanup on interruption
 * Must be called after jm_init_device
 *
 * @param fd File descriptor to cleanup on signal
 * @param sector Sector number to zero on cleanup
 */
void jm_setup_signal_handlers(int fd, uint32_t sector);

/**
 * Remove signal handlers (called automatically on cleanup)
 */
void jm_remove_signal_handlers(void);

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
 * Write zeros to a sector via SG_IO
 * Lightweight write-zeros without signal handler side effects.
 * Used to clear stale JMicron protocol artifacts before a run.
 * Does NOT close the file descriptor.
 *
 * @param fd File descriptor from jm_init_device
 * @param sector Sector number to zero
 * @return JM_SUCCESS on success, error code on failure
 */
int jm_zero_sector(int fd, uint32_t sector);

/**
 * Read a sector via normal block device I/O (not SG_IO)
 *
 * This bypasses the JMicron controller's SG_IO interception, reading what is
 * physically stored on disk (via the OS page cache / block layer). Used as the
 * authoritative safety check: if the block device shows non-zero data, real
 * user data exists at that sector and the tool must refuse to proceed.
 *
 * SG_IO may show controller mailbox state even when the physical sector is
 * empty, so block I/O is the correct source of truth for the safety check.
 *
 * @param device_path Path to device (e.g., "/dev/sdc")
 * @param sector Sector number to read
 * @param buf Output buffer (must be 512 bytes)
 * @return 0 on success, -1 on failure
 */
int jm_read_sector_block(const char *device_path, uint32_t sector, uint8_t *buf);

/**
 * Get human-readable error message for error code
 *
 * @param error_code Error code from jm_* functions
 * @return Error message string
 */
const char* jm_error_string(jm_error_code_t error_code);

#endif /* JM_PROTOCOL_H */
