#include <stdio.h>
#include <stdint.h>
#include "src/jm_protocol.h"
#include "src/jm_commands.h"

int main() {
    int fd;
    uint8_t backup[512];
    
    if (jm_init_device("/dev/sde", &fd, backup, 0x21) != 0) {
        return 1;
    }
    
    jm_send_wakeup(fd, 0x21);
    
    printf("Testing IDENTIFY disk 4 (doesn't exist)...\n");
    char model[41];
    int result = jm_get_disk_identify(fd, 4, model, NULL, NULL, 0x21);
    printf("Result: %d, Model: %s\n", result, model);
    
    jm_cleanup_device(fd, backup, 0x21);
    return 0;
}
