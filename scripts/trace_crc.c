#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "src/jm_protocol.h"
#include "src/jm_commands.h"

int main() {
    int fd;
    uint8_t backup[512];
    
    if (jm_init_device("/dev/sde", &fd, backup, 0x21) != 0) {
        return 1;
    }
    
    printf("Sending wakeup...\n");
    jm_send_wakeup(fd, 0x21);
    
    for (int i = 0; i < 5; i++) {
        printf("\n=== Disk %d ===\n", i);
        
        char model[41] = {0};
        printf("IDENTIFY disk %d...\n", i);
        int result = jm_get_disk_identify(fd, i, model, NULL, NULL, 0x21);
        printf("Result: %d, Model: %s\n", result, model);
    }
    
    jm_cleanup_device(fd, backup, 0x21);
    return 0;
}
