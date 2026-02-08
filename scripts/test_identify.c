#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <asm/byteorder.h>
#include "src/jm_protocol.h"
#include "src/jm_commands.h"

static void hexdump(const uint8_t* data, int len, const char* label) {
    printf("\n%s:\n", label);
    for (int i = 0; i < len && i < 128; i++) {
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

int main() {
    int fd;
    uint8_t backup[512];
    
    if (jm_init_device("/dev/sde", &fd, backup, 0x21) != 0) {
        return 1;
    }
    
    jm_send_wakeup(fd, 0x21);
    
    /* probe11 - IDENTIFY disk 0 */
    const uint8_t probe11[] = {0x00, 0x02, 0x02, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t cmd_buf[512] = {0};
    uint8_t resp_buf[512] = {0};
    uint32_t* cmd_buf32 = (uint32_t*)cmd_buf;
    uint32_t* resp_buf32 = (uint32_t*)resp_buf;
    
    cmd_buf32[0] = __cpu_to_le32(0x197b0322);
    cmd_buf32[1] = __cpu_to_le32(1);
    memcpy(cmd_buf + 8, probe11, sizeof(probe11));
    
    jm_execute_command(fd, cmd_buf32, resp_buf32, 0x21);
    
    hexdump(resp_buf, 128, "IDENTIFY Response (first 128 bytes)");
    
    printf("\nModel at offset 54 (no header skip):\n");
    for (int i = 54; i < 94; i++) printf("%c", resp_buf[i] >= 0x20 ? resp_buf[i] : '.');
    
    printf("\n\nModel at offset 54+32 (with 0x20 header skip):\n");
    for (int i = 54+32; i < 94+32; i++) printf("%c", resp_buf[i] >= 0x20 ? resp_buf[i] : '.');
    
    printf("\n");
    
    jm_cleanup_device(fd, backup, 0x21);
    return 0;
}
