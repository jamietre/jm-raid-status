/**
 * zero_sector.c - Emergency sector cleanup utility
 *
 * This tool writes zeros to a specific sector on a disk. It is intended
 * for emergency recovery if jmraidstatus fails to clean up its communication
 * sector (default: sector 1024) after an abnormal termination.
 *
 * **USE WITH CAUTION**: This tool can overwrite any sector on your disk,
 * potentially causing data loss if used incorrectly.
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: GPL-2.0-or-later
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

void print_usage(const char* program_name) {
    printf("Usage: %s <device> <sector_number>\n", program_name);
    printf("\n");
    printf("Overwrites the specified sector with zeros.\n");
    printf("\n");
    printf("WARNING: This can cause data loss if used on the wrong sector!\n");
    printf("         Only use this to clean up the jmraidstatus communication\n");
    printf("         sector (default: 1024) after abnormal termination.\n");
    printf("\n");
    printf("Arguments:\n");
    printf("  device         Device path (e.g., /dev/sde)\n");
    printf("  sector_number  Sector to overwrite (typically 1024)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  sudo %s /dev/sde 1024\n", program_name);
    printf("\n");
}

int write_sector_zeros(int fd, uint32_t sector) {
    uint8_t zero_sector[SECTOR_SIZE];
    memset(zero_sector, 0, SECTOR_SIZE);

    /* Build SCSI WRITE(10) command */
    uint8_t cdb[10] = {
        0x2A,                           /* WRITE(10) opcode */
        0x00,                           /* flags */
        (sector >> 24) & 0xff,          /* LBA bits 31-24 */
        (sector >> 16) & 0xff,          /* LBA bits 23-16 */
        (sector >> 8) & 0xff,           /* LBA bits 15-8 */
        sector & 0xff,                  /* LBA bits 7-0 */
        0x00,                           /* reserved */
        0x00, 0x01,                     /* transfer length: 1 block */
        0x00                            /* control */
    };

    /* Setup SG_IO request */
    sg_io_hdr_t sg_io_hdr;
    memset(&sg_io_hdr, 0, sizeof(sg_io_hdr));

    sg_io_hdr.interface_id = 'S';
    sg_io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    sg_io_hdr.cmd_len = sizeof(cdb);
    sg_io_hdr.mx_sb_len = 0;
    sg_io_hdr.dxfer_len = SECTOR_SIZE;
    sg_io_hdr.dxferp = zero_sector;
    sg_io_hdr.cmdp = cdb;
    sg_io_hdr.timeout = 5000;  /* 5 second timeout */

    if (ioctl(fd, SG_IO, &sg_io_hdr) < 0) {
        perror("ioctl(SG_IO) failed");
        return -1;
    }

    if (sg_io_hdr.status != 0) {
        fprintf(stderr, "SCSI command failed with status: 0x%02x\n", sg_io_hdr.status);
        return -1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char* device = argv[1];
    uint32_t sector = (uint32_t)atoi(argv[2]);

    /* Sanity checks */
    if (sector == 0) {
        fprintf(stderr, "ERROR: Refusing to write to sector 0 (partition table/MBR)\n");
        return 1;
    }

    /* Sector 33 (0x21) is the original JMicron default and is explicitly allowed.
     * All other sectors below 64 are system areas (boot loaders, GPT, etc.). */
    if (sector != 33 && sector < 64) {
        fprintf(stderr, "ERROR: Refusing to write to sector %u (system area, sectors 1-32 and 34-63 are reserved)\n", sector);
        fprintf(stderr, "       jmraidstatus uses sector 33 (0x21) by default.\n");
        return 1;
    }

    /* Confirm with user */
    printf("WARNING: This will overwrite sector %u on %s with zeros!\n", sector, device);
    printf("         Make sure this is the correct device and sector.\n");
    printf("\n");
    printf("Continue? (yes/no): ");

    char response[10];
    if (fgets(response, sizeof(response), stdin) == NULL) {
        fprintf(stderr, "ERROR: Failed to read response\n");
        return 1;
    }

    /* Remove newline */
    response[strcspn(response, "\n")] = 0;

    if (strcmp(response, "yes") != 0) {
        printf("Aborted.\n");
        return 0;
    }

    /* Open device */
    int fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        fprintf(stderr, "Note: This tool requires root/sudo privileges.\n");
        return 1;
    }

    /* Write zeros */
    printf("Writing zeros to sector %u...\n", sector);

    if (write_sector_zeros(fd, sector) != 0) {
        fprintf(stderr, "ERROR: Failed to write zeros to sector %u\n", sector);
        close(fd);
        return 1;
    }

    close(fd);

    printf("SUCCESS: Sector %u has been overwritten with zeros.\n", sector);
    return 0;
}
