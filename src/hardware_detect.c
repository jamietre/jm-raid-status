/*
 * hardware_detect.c - Hardware detection utilities
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "hardware_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* Check if we're running in WSL */
int is_wsl(void) {
    FILE *fp = fopen("/proc/version", "r");
    if (fp == NULL)
        return 0;

    char line[256];
    int wsl_detected = 0;
    if (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "WSL") != NULL || strstr(line, "Microsoft") != NULL) {
            wsl_detected = 1;
        }
    }
    fclose(fp);
    return wsl_detected;
}

/* Get USB vendor and product IDs for a block device */
int get_usb_ids(const char *device_path, unsigned int *vendor_id, unsigned int *product_id) {
    char *devname = strrchr(device_path, '/');
    if (devname == NULL)
        return -1;
    devname++; /* Skip the '/' */

    /* Navigate up the sysfs tree to find USB device info */
    char syspath[PATH_MAX];
    char search_path[PATH_MAX];
    char realpath_buf[PATH_MAX];

    snprintf(syspath, sizeof(syspath), "/sys/block/%s/device", devname);
    if (realpath(syspath, realpath_buf) == NULL)
        return -1;

    /* Walk up the directory tree looking for idVendor/idProduct files */
    snprintf(search_path, sizeof(search_path), "%s", realpath_buf);

    for (int i = 0; i < 10; i++) { /* Max 10 levels up */
        char vendor_path[PATH_MAX];
        char product_path[PATH_MAX];
        snprintf(vendor_path, sizeof(vendor_path), "%s/idVendor", search_path);
        snprintf(product_path, sizeof(product_path), "%s/idProduct", search_path);

        FILE *vf = fopen(vendor_path, "r");
        FILE *pf = fopen(product_path, "r");

        if (vf && pf) {
            if (fscanf(vf, "%x", vendor_id) == 1 && fscanf(pf, "%x", product_id) == 1) {
                fclose(vf);
                fclose(pf);
                return 0;
            }
        }
        if (vf)
            fclose(vf);
        if (pf)
            fclose(pf);

        /* Go up one directory */
        char *last_slash = strrchr(search_path, '/');
        if (last_slash == NULL || last_slash == search_path)
            break;
        *last_slash = '\0';
    }

    return -1;
}

/* Check if a block device is USB-connected */
int is_usb_device(const char *device_path) {
    char *devname = strrchr(device_path, '/');
    if (devname == NULL)
        return 0;
    devname++; /* Skip the '/' */

    /* Build path to device's sysfs entry */
    char syspath[PATH_MAX];
    char realpath_buf[PATH_MAX];
    snprintf(syspath, sizeof(syspath), "/sys/block/%s/device", devname);

    /* Resolve the real path */
    if (realpath(syspath, realpath_buf) == NULL)
        return 0;

    /* Check if the path contains '/usb' which indicates USB connection */
    return (strstr(realpath_buf, "/usb") != NULL) ? 1 : 0;
}

/* Map JMicron PCIe device IDs to model names */
static const char *get_jmicron_model(unsigned int device_id) {
    switch (device_id) {
    case 0x0394:
        return "JMB394";
    case 0x0393:
        return "JMB393";
    case 0x2391:
        return "JMB391";
    case 0x2390:
        return "JMB390";
    case 0x2388:
        return "JMB388";
    case 0x2385:
        return "JMB385";
    case 0x2363:
        return "JMB363";
    case 0x2362:
        return "JMB362";
    case 0x2361:
        return "JMB361";
    default:
        return "Unknown JMicron";
    }
}

/* Map USB vendor/product IDs to JMicron controller models */
static const char *get_usb_controller_model(unsigned int vendor_id, unsigned int product_id) {
    /* JMicron USB vendor ID (0x152d) - different from PCIe vendor ID */
    if (vendor_id == 0x152d) {
        switch (product_id) {
        case 0x0567:
            return "JMB567";
        case 0x0578:
            return "JMB578";
        case 0x1561:
            return "JMB561";
        case 0x1562:
            return "JMB562";
        case 0x0575:
            return "JMB575";
        case 0x0576:
            return "JMB576";
        default:
            return "JMicron USB RAID";
        }
    }

    /* JMicron PCIe vendor ID seen on USB (rare) */
    if (vendor_id == 0x197b) {
        switch (product_id) {
        case 0x0394:
            return "JMB394";
        case 0x0393:
            return "JMB393";
        case 0x2394:
            return "JMB394 (USB)";
        default:
            return "JMicron RAID";
        }
    }

    return NULL; /* Unknown USB controller */
}

/* Check if JMicron controller is present and identify model */
int detect_jmicron_hardware(controller_info_t *info, const char *device_path) {
    memset(info, 0, sizeof(controller_info_t));

    /* In WSL, PCI devices aren't visible but may be passed through */
    if (is_wsl()) {
        info->found = 1;
        snprintf(info->model, sizeof(info->model), "JMicron (WSL)");
        snprintf(info->description, sizeof(info->description),
                 "Controller detection skipped in WSL environment");
        return 0;
    }

    /* Check if this is a USB device - most JMicron RAID enclosures are USB */
    if (is_usb_device(device_path)) {
        unsigned int usb_vendor = 0, usb_product = 0;
        info->found = 1;

        /* Try to get USB vendor/product IDs */
        if (get_usb_ids(device_path, &usb_vendor, &usb_product) == 0) {
            info->vendor_id = usb_vendor;
            info->device_id = usb_product;

            const char *model = get_usb_controller_model(usb_vendor, usb_product);
            if (model != NULL) {
                snprintf(info->model, sizeof(info->model), "%s", model);
                snprintf(info->description, sizeof(info->description),
                         "USB enclosure (VID:%04x PID:%04x)", usb_vendor, usb_product);
            } else {
                snprintf(info->model, sizeof(info->model), "USB enclosure");
                snprintf(info->description, sizeof(info->description),
                         "USB-connected storage (VID:%04x PID:%04x)", usb_vendor, usb_product);
            }
        } else {
            snprintf(info->model, sizeof(info->model), "USB enclosure");
            snprintf(info->description, sizeof(info->description),
                     "USB-connected storage (likely JMicron RAID enclosure)");
        }
        return 0;
    }

    /* Check for JMicron (197b) SATA controllers (class 0106) */
    FILE *fp = popen("lspci -n -d 197b: 2>/dev/null", "r");
    if (fp == NULL) {
        return -1; // Cannot run lspci
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* Parse lspci output: "XX:XX.X 0106: 197b:0394 (rev 03)" */
        unsigned int vendor, device;
        if (sscanf(line, "%*s %*s %x:%x", &vendor, &device) == 2) {
            if (vendor == 0x197b) {
                info->found = 1;
                info->vendor_id = vendor;
                info->device_id = device;
                snprintf(info->model, sizeof(info->model), "%s",
                         get_jmicron_model(device));
                break;
            }
        }
    }
    pclose(fp);

    /* If found, get full description from lspci */
    if (info->found) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "lspci -d %04x:%04x 2>/dev/null",
                 info->vendor_id, info->device_id);
        fp = popen(cmd, "r");
        if (fp != NULL) {
            if (fgets(line, sizeof(line), fp) != NULL) {
                /* Extract description after device ID */
                char *desc = strchr(line, ':');
                if (desc != NULL) {
                    desc++; // Skip the colon
                    while (*desc == ' ')
                        desc++; // Skip spaces
                    /* Remove newline */
                    char *newline = strchr(desc, '\n');
                    if (newline)
                        *newline = '\0';
                    snprintf(info->description, sizeof(info->description), "%s", desc);
                }
            }
            pclose(fp);
        }
    }

    return info->found ? 0 : -1;
}
