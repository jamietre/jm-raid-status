#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "src/jm_crc.h"
#include "src/jm_protocol.h"
#include "src/jm_commands.h"
#include "src/smart_parser.h"

int main() {
    int fd;
    uint8_t backup[512];
    
    if (jm_init_device("/dev/sde", &fd, backup, 0x21) != 0) {
        printf("Failed to init\n");
        return 1;
    }
    
    printf("Sending wakeup...\n");
    jm_send_wakeup(fd, 0x21);
    
    printf("\nQuerying all disks (like the real tool)...\n");
    disk_smart_data_t data[5];
    int num_disks;
    int result = jm_get_all_disks_smart_data(fd, data, &num_disks, 0x21);
    
    printf("Result: %d, Found %d disks\n", result, num_disks);
    
    for (int i = 0; i < 5; i++) {
        if (data[i].is_present) {
            printf("Disk %d: %d attributes, status=%d\n", 
                   i, data[i].num_attributes, data[i].overall_status);
        }
    }
    
    jm_cleanup_device(fd, backup, 0x21);
    return 0;
}
