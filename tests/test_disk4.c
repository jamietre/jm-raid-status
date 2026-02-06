#include <stdio.h>
#include "src/jm_protocol.h"
#include "src/jm_commands.h"
#include "src/smart_parser.h"
#include <string.h>

int main() {
    int fd;
    uint8_t backup[512];
    
    if (jm_init_device("/dev/sde", &fd, backup, 0x21) != 0) {
        printf("Failed to init\n");
        return 1;
    }
    
    jm_send_wakeup(fd, 0x21);
    
    disk_smart_data_t data;
    int result = jm_get_disk_smart_data(fd, 4, NULL, &data, 0x21);
    
    printf("Disk 4 result: %d\n", result);
    printf("Disk 4 is_present: %d\n", data.is_present);
    printf("Disk 4 num_attributes: %d\n", data.num_attributes);
    printf("Disk 4 status: %d\n", data.overall_status);
    
    jm_cleanup_device(fd, backup, 0x21);
    return 0;
}
