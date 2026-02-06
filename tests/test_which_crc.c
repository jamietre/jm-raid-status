#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <asm/byteorder.h>
#include "src/jm_protocol.h"
#include "src/jm_commands.h"

int main() {
    int fd;
    uint8_t backup[512];
    
    if (jm_init_device("/dev/sde", &fd, backup, 0x21) != 0) {
        return 1;
    }
    
    jm_send_wakeup(fd, 0x21);
    
    char model[41], serial[21], firmware[9];
    
    printf("Testing IDENTIFY disk 0...\n");
    jm_get_disk_identify(fd, 0, model, serial, firmware, 0x21);
    printf("Model: %s\n", model);
    
    printf("\nTesting SMART VALUES disk 0...\n");
    smart_values_page_t values;
    jm_smart_read_values(fd, 0, &values, 0x21);
    printf("Got SMART values\n");
    
    jm_cleanup_device(fd, backup, 0x21);
    return 0;
}
