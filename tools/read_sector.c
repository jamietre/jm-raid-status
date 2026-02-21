/**
 * read_sector.c - Diagnostic: read a sector via SG_IO and dump as hex
 *
 * Reads a sector exactly the same way jmraidstatus does (via SG_IO SCSI
 * pass-through), bypassing the OS block device / page cache. Used to
 * diagnose what the JMicron controller actually returns for a given sector.
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

#define SECTOR_SIZE 512

static void hexdump(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i += 16) {
        printf("%04zx: ", i);
        for (size_t j = 0; j < 16 && i + j < len; j++)
            printf("%02x ", buf[i + j]);
        /* pad if last row is short */
        for (size_t j = len - i; j < 16; j++)
            printf("   ");
        printf(" |");
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            uint8_t c = buf[i + j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("|\n");
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <device> <sector>\n", argv[0]);
        fprintf(stderr, "  Reads a sector via SG_IO (same as jmraidstatus) and dumps it.\n");
        fprintf(stderr, "  Example: sudo %s /dev/usb1 33\n", argv[0]);
        return 1;
    }

    const char *device = argv[1];
    uint32_t sector = (uint32_t)strtoul(argv[2], NULL, 0);

    int fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    /* Verify SG_IO is available */
    int sg_version;
    if (ioctl(fd, SG_GET_VERSION_NUM, &sg_version) < 0 || sg_version < 30000) {
        fprintf(stderr, "Error: %s does not support SG_IO (not an SG device or old driver)\n", device);
        close(fd);
        return 1;
    }

    /* Build SCSI READ(10) command - identical to jm_init_device */
    uint8_t cdb[10] = {
        0x28,                           /* READ(10) opcode */
        0x00,                           /* flags */
        (sector >> 24) & 0xff,          /* LBA bits 31-24 */
        (sector >> 16) & 0xff,          /* LBA bits 23-16 */
        (sector >>  8) & 0xff,          /* LBA bits 15-8  */
        sector & 0xff,                  /* LBA bits 7-0   */
        0x00,                           /* reserved */
        0x00, 0x01,                     /* transfer length: 1 block */
        0x00                            /* control */
    };

    uint8_t buf[SECTOR_SIZE];
    uint8_t sense[32];
    memset(buf, 0, sizeof(buf));
    memset(sense, 0, sizeof(sense));

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.interface_id    = 'S';
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.cmd_len         = sizeof(cdb);
    io_hdr.mx_sb_len       = sizeof(sense);
    io_hdr.dxfer_len       = SECTOR_SIZE;
    io_hdr.dxferp          = buf;
    io_hdr.cmdp            = cdb;
    io_hdr.sbp             = sense;
    io_hdr.timeout         = 3000;

    if (ioctl(fd, SG_IO, &io_hdr) < 0) {
        perror("ioctl(SG_IO)");
        close(fd);
        return 1;
    }

    close(fd);

    if (io_hdr.status != 0) {
        fprintf(stderr, "SCSI status: 0x%02x\n", io_hdr.status);
        if (io_hdr.sb_len_wr > 0) {
            fprintf(stderr, "Sense data: ");
            for (int i = 0; i < io_hdr.sb_len_wr; i++)
                fprintf(stderr, "%02x ", sense[i]);
            fprintf(stderr, "\n");
        }
        return 1;
    }

    /* Check if all zeros */
    int all_zero = 1;
    for (int i = 0; i < SECTOR_SIZE; i++) {
        if (buf[i] != 0) { all_zero = 0; break; }
    }

    printf("Sector %u on %s (via SG_IO): %s\n\n",
           sector, device, all_zero ? "ALL ZEROS (empty)" : "CONTAINS DATA");

    hexdump(buf, SECTOR_SIZE);

    /* Identify JMicron protocol signatures */
    if (!all_zero) {
        uint32_t word0 = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
        printf("\nInterpretation:\n");
        if (word0 == 0x197b0325)
            printf("  -> JMicron WAKEUP packet (magic 0x197b0325) - leftover from interrupted run\n");
        else if (word0 == 0x197b0322)
            printf("  -> JMicron COMMAND/RESPONSE header (unscrambled, magic 0x197b0322)\n");
        else
            printf("  -> First 4 bytes: 0x%08x - not a known JMicron magic number\n", word0);
    }

    return all_zero ? 0 : 1;
}
