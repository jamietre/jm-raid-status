/* Quick debug tool to dump raw SMART responses */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <asm/byteorder.h>
#include "src/jm_protocol.h"
#include "src/jm_commands.h"

static void hexdump(const uint8_t* data, int len, const char* label) {
    printf("\n%s:\n", label);
    for (int i = 0; i < len; i++) {
        if (i % 16 == 0) printf("%04x: ", i);
        printf("%02x ", data[i]);
        if (i % 16 == 15) {
            printf(" |");
            for (int j = i - 15; j <= i; j++) {
                printf("%c", (data[j] >= 0x20 && data[j] < 0x7f) ? data[j] : '.');
            }
            printf("|\n");
        }
    }
    printf("\n");
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s /dev/sdX\n", argv[0]);
        return 1;
    }

    int fd;
    uint8_t backup[512];
    uint32_t sector = 0x21;

    printf("Opening device...\n");
    if (jm_init_device(argv[1], &fd, backup, sector) != 0) {
        fprintf(stderr, "Failed to open device\n");
        return 1;
    }

    printf("Sending wakeup...\n");
    if (jm_send_wakeup(fd, sector) != 0) {
        fprintf(stderr, "Failed to send wakeup\n");
        return 1;
    }

    /* Manually execute probe23 (disk 0 SMART values) */
    const uint8_t probe23[] = {
        0x00, 0x02, 0x03, 0xff, 0x00, 0x02, 0x00, 0xe0, 0x00, 0x00,
        0xd0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f, 0x00, 0xc2, 0x00, 0xa0, 0x00, 0xb0, 0x00
    };

    uint8_t cmd_buf[512] = {0};
    uint8_t resp_buf[512] = {0};
    uint32_t* cmd_buf32 = (uint32_t*)cmd_buf;
    uint32_t* resp_buf32 = (uint32_t*)resp_buf;

    cmd_buf32[0] = __cpu_to_le32(0x197b0322);  /* JM_RAID_SCRAMBLED_CMD */
    cmd_buf32[1] = __cpu_to_le32(1);           /* command counter */
    memcpy(cmd_buf + 8, probe23, sizeof(probe23));

    printf("Executing SMART read command...\n");
    jm_execute_command(fd, cmd_buf32, resp_buf32, sector);

    hexdump(resp_buf, 512, "Raw SMART Response");

    /* Also dump probe24 (thresholds) */
    const uint8_t probe24[] = {
        0x00, 0x02, 0x03, 0xff, 0x00, 0x02, 0x00, 0xe0, 0x00, 0x00,
        0xd1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f, 0x00, 0xc2, 0x00, 0xa0, 0x00, 0xb0, 0x00
    };

    memset(cmd_buf, 0, 512);
    cmd_buf32[0] = __cpu_to_le32(0x197b0322);
    cmd_buf32[1] = __cpu_to_le32(2);
    memcpy(cmd_buf + 8, probe24, sizeof(probe24));

    jm_execute_command(fd, cmd_buf32, resp_buf32, sector);
    hexdump(resp_buf, 512, "Raw SMART Thresholds");

    jm_cleanup_device(fd, backup, sector);
    return 0;
}
