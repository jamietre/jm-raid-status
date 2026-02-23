/*
 * jm_protocol.c - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "jm_protocol.h"
#include "jm_crc.h"
#include "sata_xor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <asm/byteorder.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>

#define READ_CMD (0x28)
#define WRITE_CMD (0x2a)
#define RW_CMD_LEN (10)

#define JM_RAID_WAKEUP_CMD    (0x197b0325)
#define JM_RAID_SCRAMBLED_CMD (0x197b0322)

/* Static SG_IO header - reused for all operations */
static sg_io_hdr_t sg_io_hdr;
static uint8_t rw_cmd_blk[RW_CMD_LEN];

/* Global state for signal handlers - ensures cleanup on interruption */
static volatile sig_atomic_t g_cleanup_fd = -1;
static volatile sig_atomic_t g_cleanup_sector = 0;
static volatile sig_atomic_t g_cleanup_done = 0;

/**
 * Signal handler - writes zeros to communication sector and exits
 * This is called on SIGINT (Ctrl+C), SIGTERM, etc.
 */
static void jm_signal_handler(int signum) {
    /* If cleanup already done or not initialized, just exit */
    if (g_cleanup_done || g_cleanup_fd < 0) {
        _exit(128 + signum);
    }

    /* Mark cleanup as done (prevents re-entry) */
    g_cleanup_done = 1;

    /* Write zeros to sector - use direct write, avoid jm_cleanup_device
     * to keep signal handler simple and async-signal-safe */
    uint8_t zero_sector[512];
    memset(zero_sector, 0, 512);

    /* Setup write command */
    uint8_t cmd_blk[10];
    memset(cmd_blk, 0, 10);
    cmd_blk[0] = WRITE_CMD;
    cmd_blk[5] = g_cleanup_sector & 0xFF;
    cmd_blk[4] = (g_cleanup_sector >> 8) & 0xFF;
    cmd_blk[3] = (g_cleanup_sector >> 16) & 0xFF;
    cmd_blk[2] = (g_cleanup_sector >> 24) & 0xFF;
    cmd_blk[8] = 0x01;  // Number of sectors

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.cmd_len = 10;
    io_hdr.dxfer_len = 512;
    io_hdr.dxferp = zero_sector;
    io_hdr.cmdp = cmd_blk;
    io_hdr.timeout = 3000;

    /* Best effort write - ignore errors in signal handler */
    ioctl(g_cleanup_fd, SG_IO, &io_hdr);

    /* Exit with signal-specific exit code */
    _exit(128 + signum);
}

/**
 * Setup signal handlers to ensure cleanup on interruption
 */
void jm_setup_signal_handlers(int fd, uint32_t sector) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = jm_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;  /* Only catch first signal */

    /* Store for signal handler */
    g_cleanup_fd = fd;
    g_cleanup_sector = sector;
    g_cleanup_done = 0;

    /* Install handlers for catchable termination signals */
    sigaction(SIGINT, &sa, NULL);   /* Ctrl+C */
    sigaction(SIGTERM, &sa, NULL);  /* kill command */
    sigaction(SIGHUP, &sa, NULL);   /* Terminal hangup */
    sigaction(SIGQUIT, &sa, NULL);  /* Ctrl+\ */
}

/**
 * Remove signal handlers (restore default behavior)
 */
void jm_remove_signal_handlers(void) {
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    g_cleanup_fd = -1;
}
static uint8_t sense_buffer[32];

const char* jm_error_string(jm_error_code_t error_code) {
    switch (error_code) {
        case JM_SUCCESS:
            return "Success";
        case JM_ERROR_DEVICE_OPEN:
            return "Cannot open device";
        case JM_ERROR_NOT_SG_DEVICE:
            return "Not an SG device or old SG driver";
        case JM_ERROR_IOCTL_FAILED:
            return "IOCTL operation failed";
        case JM_ERROR_CRC_MISMATCH:
            return "Response CRC mismatch";
        case JM_ERROR_INVALID_ARGS:
            return "Invalid arguments";
        default:
            return "Unknown error";
    }
}

int jm_init_device(const char* device_path, int* fd_out) {
    int fd;
    int sg_version;

    if (device_path == NULL || fd_out == NULL) {
        return JM_ERROR_INVALID_ARGS;
    }

    /* Open device */
    fd = open(device_path, O_RDWR);
    if (fd < 0) {
        return JM_ERROR_DEVICE_OPEN;
    }

    /* Verify it's an SG device with sufficient driver version */
    if ((ioctl(fd, SG_GET_VERSION_NUM, &sg_version) < 0) || (sg_version < 30000)) {
        close(fd);
        return JM_ERROR_NOT_SG_DEVICE;
    }

    /* Setup SG_IO header for subsequent operations */
    memset(&sg_io_hdr, 0, sizeof(sg_io_hdr_t));
    sg_io_hdr.interface_id = 'S';
    sg_io_hdr.cmd_len = RW_CMD_LEN;
    sg_io_hdr.mx_sb_len = sizeof(sense_buffer);
    sg_io_hdr.dxfer_len = JM_SECTORSIZE;
    sg_io_hdr.cmdp = rw_cmd_blk;
    sg_io_hdr.sbp = sense_buffer;
    sg_io_hdr.timeout = 3000;

    memset(rw_cmd_blk, 0, RW_CMD_LEN);
    rw_cmd_blk[8] = 0x01;  /* Number of sectors */

    *fd_out = fd;
    return JM_SUCCESS;
}

int jm_cleanup_device(int fd, uint32_t sector) {
    if (fd < 0) {
        return JM_ERROR_INVALID_ARGS;
    }

    /* Mark cleanup as done to prevent signal handler from running */
    g_cleanup_done = 1;

    /* Idempotent - safe to call multiple times */
    if (g_cleanup_fd == -1) {
        return JM_SUCCESS;  /* Already cleaned up */
    }

    /* Remove signal handlers */
    jm_remove_signal_handlers();

    /* Write zeros to sector (restore to verified-safe state) */
    static uint8_t zero_sector[512];
    memset(zero_sector, 0, 512);

    /* Update command block for write */
    rw_cmd_blk[0] = WRITE_CMD;
    rw_cmd_blk[5] = sector & 0xFF;
    rw_cmd_blk[4] = (sector >> 8) & 0xFF;
    rw_cmd_blk[3] = (sector >> 16) & 0xFF;
    rw_cmd_blk[2] = (sector >> 24) & 0xFF;

    /* Write zeros to sector */
    sg_io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    sg_io_hdr.dxferp = zero_sector;

    if (ioctl(fd, SG_IO, &sg_io_hdr) < 0) {
        close(fd);
        return JM_ERROR_IOCTL_FAILED;
    }

    close(fd);
    g_cleanup_fd = -1;  /* Mark as cleaned up */
    return JM_SUCCESS;
}

int jm_zero_sector(int fd, uint32_t sector) {
    if (fd < 0) {
        return JM_ERROR_INVALID_ARGS;
    }

    uint8_t zero_buf[512];
    memset(zero_buf, 0, 512);

    uint8_t cmd_blk[10];
    memset(cmd_blk, 0, 10);
    cmd_blk[0] = WRITE_CMD;
    cmd_blk[5] = sector & 0xFF;
    cmd_blk[4] = (sector >> 8) & 0xFF;
    cmd_blk[3] = (sector >> 16) & 0xFF;
    cmd_blk[2] = (sector >> 24) & 0xFF;
    cmd_blk[8] = 0x01;

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.interface_id    = 'S';
    io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr.cmd_len         = 10;
    io_hdr.dxfer_len       = 512;
    io_hdr.dxferp          = zero_buf;
    io_hdr.cmdp            = cmd_blk;
    io_hdr.timeout         = 3000;

    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        return JM_ERROR_IOCTL_FAILED;
    }

    return JM_SUCCESS;
}

int jm_send_wakeup(int fd, uint32_t sector) {
    uint8_t wakeup_buf[JM_SECTORSIZE];
    uint32_t* wakeup_buf32 = (uint32_t*)wakeup_buf;

    /* Wakeup sequence constants */
    const uint32_t wakeup_values[] = {0x3c75a80b, 0x0388e337, 0x689705f3, 0xe00c523a};

    /* Update command block for write */
    rw_cmd_blk[0] = WRITE_CMD;
    rw_cmd_blk[5] = sector & 0xFF;
    rw_cmd_blk[4] = (sector >> 8) & 0xFF;
    rw_cmd_blk[3] = (sector >> 16) & 0xFF;
    rw_cmd_blk[2] = (sector >> 24) & 0xFF;

    sg_io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    sg_io_hdr.dxferp = wakeup_buf;

    /* Send 4 wakeup sectors */
    for (int i = 0; i < 4; i++) {
        memset(wakeup_buf, 0, JM_SECTORSIZE);

        /* Setup wakeup command structure */
        wakeup_buf32[0] = __cpu_to_le32(JM_RAID_WAKEUP_CMD);
        wakeup_buf32[1] = __cpu_to_le32(wakeup_values[i]);
        wakeup_buf32[0x1f8 >> 2] = __cpu_to_le32(0x10eca1db);

        /* Fill pattern from 0x10 to 0x1f7 */
        for (uint32_t j = 0x10; j < 0x1f8; j++) {
            wakeup_buf[j] = j & 0xff;
        }

        /* Calculate and add CRC */
        uint32_t crc = JM_CRC(wakeup_buf32, 0x1fc >> 2);
        wakeup_buf32[0x1fc >> 2] = __cpu_to_le32(crc);

        /* Send wakeup sector */
        if (ioctl(fd, SG_IO, &sg_io_hdr) < 0) {
            return JM_ERROR_IOCTL_FAILED;
        }
    }

    return JM_SUCCESS;
}

int jm_execute_command(int fd, uint32_t* cmd_buf, uint32_t* resp_buf, uint32_t sector) {
    if (fd < 0 || cmd_buf == NULL || resp_buf == NULL) {
        return JM_ERROR_INVALID_ARGS;
    }

    /* Calculate CRC for the request */
    uint32_t crc = JM_CRC(cmd_buf, 0x7f);
    cmd_buf[0x7f] = __cpu_to_le32(crc);

    /* Apply XOR scrambling */
    SATA_XOR(cmd_buf);

    /* Update command block for sector */
    rw_cmd_blk[5] = sector & 0xFF;
    rw_cmd_blk[4] = (sector >> 8) & 0xFF;
    rw_cmd_blk[3] = (sector >> 16) & 0xFF;
    rw_cmd_blk[2] = (sector >> 24) & 0xFF;

    /* Send command (write) */
    rw_cmd_blk[0] = WRITE_CMD;
    sg_io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    sg_io_hdr.dxferp = cmd_buf;

    if (ioctl(fd, SG_IO, &sg_io_hdr) < 0) {
        return JM_ERROR_IOCTL_FAILED;
    }

    /* Read response */
    rw_cmd_blk[0] = READ_CMD;
    sg_io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    sg_io_hdr.dxferp = resp_buf;

    if (ioctl(fd, SG_IO, &sg_io_hdr) < 0) {
        return JM_ERROR_IOCTL_FAILED;
    }

    /* Remove XOR scrambling from response */
    SATA_XOR(resp_buf);

    /* Verify response CRC */
    crc = JM_CRC(resp_buf, 0x7f);
    if (crc != __le32_to_cpu(resp_buf[0x7f])) {
        fprintf(stderr, "Warning: Response CRC 0x%08x does not match calculated 0x%08x\n",
                __le32_to_cpu(resp_buf[0x7f]), crc);
        return JM_ERROR_CRC_MISMATCH;
    }

    return JM_SUCCESS;
}

int jm_read_sector_block(const char *device_path, uint32_t sector, uint8_t *buf)
{
    if (!device_path || !buf)
        return -1;

    int fd = open(device_path, O_RDONLY);
    if (fd < 0)
        return -1;

    off_t offset = (off_t)sector * JM_SECTORSIZE;
    if (lseek(fd, offset, SEEK_SET) < 0) {
        close(fd);
        return -1;
    }

    ssize_t n = read(fd, buf, JM_SECTORSIZE);
    close(fd);

    return (n == JM_SECTORSIZE) ? 0 : -1;
}
