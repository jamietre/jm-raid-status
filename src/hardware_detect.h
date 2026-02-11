/*
 * hardware_detect.h - Hardware detection utilities
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef HARDWARE_DETECT_H
#define HARDWARE_DETECT_H

/* Controller information structure */
typedef struct {
    int found;                  /* 1 if controller found, 0 otherwise */
    unsigned int vendor_id;     /* PCI/USB vendor ID */
    unsigned int device_id;     /* PCI/USB device/product ID */
    char model[64];             /* Controller model (e.g., "JMB567") */
    char description[256];      /* Full description from lspci/sysfs */
} controller_info_t;

/**
 * Detect JMicron RAID controller hardware
 * Works with both USB and PCIe JMicron controllers
 *
 * @param info Output controller information
 * @param device_path Device path to check (e.g., "/dev/sdc")
 * @return 0 on success (found=1 if detected), -1 on error
 */
int detect_jmicron_hardware(controller_info_t* info, const char* device_path);

/**
 * Check if running in Windows Subsystem for Linux
 * @return 1 if WSL, 0 otherwise
 */
int is_wsl(void);

/**
 * Check if a block device is USB-connected
 * @param device_path Device path (e.g., "/dev/sdc")
 * @return 1 if USB, 0 otherwise
 */
int is_usb_device(const char* device_path);

/**
 * Get USB vendor and product IDs for a block device
 * @param device_path Device path (e.g., "/dev/sdc")
 * @param vendor_id Output vendor ID
 * @param product_id Output product ID
 * @return 0 on success, -1 on error
 */
int get_usb_ids(const char* device_path, unsigned int* vendor_id, unsigned int* product_id);

#endif /* HARDWARE_DETECT_H */
