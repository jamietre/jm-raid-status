#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "src/jm_crc.h"
#include "src/jm_protocol.h"
#include "src/jm_commands.h"
#include <asm/byteorder.h>

int main() {
    int fd;
    uint8_t backup[512];
    
    if (jm_init_device("/dev/sde", &fd, backup, 0x21) != 0) {
        printf("Failed to init\n");
        return 1;
    }
    
    jm_send_wakeup(fd, 0x21);
    
    /* Execute probe23 manually */
    const uint8_t probe23[] = {
        0x00, 0x02, 0x03, 0xff, 0x00, 0x02, 0x00, 0xe0, 0x00, 0x00,
        0xd0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f, 0x00, 0xc2, 0x00, 0xa0, 0x00, 0xb0, 0x00
    };
    
    uint8_t cmd_buf[512] = {0};
    uint8_t resp_buf[512] = {0};
    uint32_t* cmd_buf32 = (uint32_t*)cmd_buf;
    uint32_t* resp_buf32 = (uint32_t*)resp_buf;
    
    cmd_buf32[0] = __cpu_to_le32(0x197b0322);
    cmd_buf32[1] = __cpu_to_le32(1);
    memcpy(cmd_buf + 8, probe23, sizeof(probe23));
    
    jm_execute_command(fd, cmd_buf32, resp_buf32, 0x21);
    
    /* Try different CRC calculations */
    printf("\nAnalyzing response CRC:\n");
    printf("Response CRC at offset 0x1FC: 0x%08x\n", __le32_to_cpu(resp_buf32[0x7f]));
    
    /* Standard calculation (0x7f dwords) */
    uint32_t crc1 = JM_CRC(resp_buf32, 0x7f);
    printf("CRC of first 0x7f dwords: 0x%08x %s\n", crc1,
           (crc1 == __le32_to_cpu(resp_buf32[0x7f])) ? "MATCH!" : "");
    
    /* Try without the first 8 bytes (header) */
    uint32_t crc2 = JM_CRC(resp_buf32 + 2, 0x7f - 2);
    printf("CRC of dwords 2-0x7f: 0x%08x %s\n", crc2,
           (crc2 == __le32_to_cpu(resp_buf32[0x7f])) ? "MATCH!" : "");
    
    /* Try calculating from offset 0x20 (where SMART data starts) */
    uint32_t crc3 = JM_CRC(resp_buf32 + 8, 0x7f - 8);
    printf("CRC of dwords 8-0x7f: 0x%08x %s\n", crc3,
           (crc3 == __le32_to_cpu(resp_buf32[0x7f])) ? "MATCH!" : "");
    
    /* Maybe response CRC should be 0? */
    printf("\nFirst 16 bytes of response:\n");
    for (int i = 0; i < 16; i++) {
        printf("%02x ", resp_buf[i]);
    }
    printf("\n");
    
    jm_cleanup_device(fd, backup, 0x21);
    return 0;
}
