/*
 * jmraidstatus.c - Part of jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include "jm_protocol.h"
#include "jm_commands.h"
#include "smart_parser.h"
#include "output_formatter.h"

#define VERSION "1.0"
#define DEFAULT_SECTOR 1024 /* Was 0x21 (33) in original - changed to 1024 for safety */

/* Command-line options structure */
typedef struct
{
    char device_path[256];
    int disk_number; // -1 = all disks
    output_mode_t output_mode;
    int verbose;
    int quiet;
    int force; // Skip hardware detection
    uint32_t sector;
} cli_options_t;

/* Check if we're running in WSL */
static int is_wsl(void)
{
    FILE *fp = fopen("/proc/version", "r");
    if (fp == NULL)
        return 0;

    char line[256];
    int wsl_detected = 0;
    if (fgets(line, sizeof(line), fp) != NULL)
    {
        if (strstr(line, "WSL") != NULL || strstr(line, "Microsoft") != NULL)
        {
            wsl_detected = 1;
        }
    }
    fclose(fp);
    return wsl_detected;
}

/* Get USB vendor and product IDs for a block device */
static int get_usb_ids(const char *device_path, unsigned int *vendor_id, unsigned int *product_id)
{
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

    for (int i = 0; i < 10; i++)
    { /* Max 10 levels up */
        char vendor_path[PATH_MAX];
        char product_path[PATH_MAX];
        snprintf(vendor_path, sizeof(vendor_path), "%s/idVendor", search_path);
        snprintf(product_path, sizeof(product_path), "%s/idProduct", search_path);

        FILE *vf = fopen(vendor_path, "r");
        FILE *pf = fopen(product_path, "r");

        if (vf && pf)
        {
            if (fscanf(vf, "%x", vendor_id) == 1 && fscanf(pf, "%x", product_id) == 1)
            {
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
static int is_usb_device(const char *device_path)
{
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

/* Check if a sector contains all zeros (is unused) */
static int is_sector_empty(const uint8_t *sector_data, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        if (sector_data[i] != 0)
            return 0;
    }
    return 1;
}

/* Validate sector number is in a safe range */
static int is_sector_in_safe_range(uint32_t sector)
{
    /* Allow 0x21 (33) for backwards compatibility - original JMRaidCon default */
    if (sector == 0x21)
    {
        return 1;
    }

    /* Reject sectors 0-63: MBR, partition table, GPT protective MBR, boot loaders */
    /* (except 0x21 which we allow above) */
    if (sector < 64)
    {
        return 0;
    }

    /* Reject sector 2048 and above (typical first partition start) */
    if (sector >= 2048)
    {
        return 0;
    }

    return 1;
}

/* Controller information structure */
typedef struct
{
    int found;
    unsigned int vendor_id;
    unsigned int device_id;
    char model[64];
    char description[256];
} controller_info_t;

/* Map JMicron PCIe device IDs to model names */
static const char *get_jmicron_model(unsigned int device_id)
{
    switch (device_id)
    {
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
static const char *get_usb_controller_model(unsigned int vendor_id, unsigned int product_id)
{
    /* JMicron USB vendor ID (0x152d) - different from PCIe vendor ID */
    if (vendor_id == 0x152d)
    {
        switch (product_id)
        {
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
    if (vendor_id == 0x197b)
    {
        switch (product_id)
        {
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
static int detect_jmicron_hardware(controller_info_t *info, const char *device_path)
{
    memset(info, 0, sizeof(controller_info_t));

    /* In WSL, PCI devices aren't visible but may be passed through */
    if (is_wsl())
    {
        info->found = 1;
        snprintf(info->model, sizeof(info->model), "JMicron (WSL)");
        snprintf(info->description, sizeof(info->description),
                 "Controller detection skipped in WSL environment");
        return 0;
    }

    /* Check if this is a USB device - most JMicron RAID enclosures are USB */
    if (is_usb_device(device_path))
    {
        unsigned int usb_vendor = 0, usb_product = 0;
        info->found = 1;

        /* Try to get USB vendor/product IDs */
        if (get_usb_ids(device_path, &usb_vendor, &usb_product) == 0)
        {
            info->vendor_id = usb_vendor;
            info->device_id = usb_product;

            const char *model = get_usb_controller_model(usb_vendor, usb_product);
            if (model != NULL)
            {
                snprintf(info->model, sizeof(info->model), "%s", model);
                snprintf(info->description, sizeof(info->description),
                         "USB enclosure (VID:%04x PID:%04x)", usb_vendor, usb_product);
            }
            else
            {
                snprintf(info->model, sizeof(info->model), "USB enclosure");
                snprintf(info->description, sizeof(info->description),
                         "USB-connected storage (VID:%04x PID:%04x)", usb_vendor, usb_product);
            }
        }
        else
        {
            snprintf(info->model, sizeof(info->model), "USB enclosure");
            snprintf(info->description, sizeof(info->description),
                     "USB-connected storage (likely JMicron RAID enclosure)");
        }
        return 0;
    }

    /* Check for JMicron (197b) SATA controllers (class 0106) */
    FILE *fp = popen("lspci -n -d 197b: 2>/dev/null", "r");
    if (fp == NULL)
    {
        return -1; // Cannot run lspci
    }

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        /* Parse lspci output: "XX:XX.X 0106: 197b:0394 (rev 03)" */
        unsigned int vendor, device;
        if (sscanf(line, "%*s %*s %x:%x", &vendor, &device) == 2)
        {
            if (vendor == 0x197b)
            {
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
    if (info->found)
    {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "lspci -d %04x:%04x 2>/dev/null",
                 info->vendor_id, info->device_id);
        fp = popen(cmd, "r");
        if (fp != NULL)
        {
            if (fgets(line, sizeof(line), fp) != NULL)
            {
                /* Extract description after device ID */
                char *desc = strchr(line, ':');
                if (desc != NULL)
                {
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

static void print_version(void)
{
    printf("jmraidstatus version %s\n", VERSION);
    printf("Copyright (C) 2026 Jamie Treworgy\n");
    printf("SPDX-License-Identifier: MIT\n\n");
    printf("https://github.com/jamietre/jm-raid-status\n");
}

static void print_help(const char *program_name)
{
    printf("Usage: %s [OPTIONS] /dev/sdX\n\n", program_name);
    printf("SMART health monitor for JMicron RAID controllers\n");
    printf("Supports USB-connected enclosures and PCIe controllers (JMB3xx series)\n\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version information\n");
    printf("  -d, --disk DISK         Query specific disk (0-4)\n");
    printf("  -a, --all               Query all disks (default)\n");
    printf("  -s, --summary           Show summary only (default)\n");
    printf("  -f, --full              Show full SMART attribute table\n");
    printf("  -j, --json              Output in JSON format\n");
    printf("  -r, --raw               Show raw hex output (backward compatibility)\n");
    printf("  -q, --quiet             Minimal output (exit code only)\n");
    printf("  --verbose               Verbose output with debug info\n");
    printf("  --force                 Skip hardware detection (use with caution)\n");
    printf("  --sector SECTOR         Use specific sector number (default: %u)\n", DEFAULT_SECTOR);
    printf("\nExamples:\n");
    printf("  %s /dev/sdc              # Show summary for all disks\n", program_name);
    printf("  %s -d 0 -f /dev/sdc      # Full SMART table for disk 0\n", program_name);
    printf("  %s -a -j /dev/sdc        # JSON output for all disks\n", program_name);
    printf("  %s --raw /dev/sdc        # Raw hex (original behavior)\n", program_name);
    printf("\nExit codes:\n");
    printf("  0: All disks healthy\n");
    printf("  1: Failed condition detected (or degraded RAID)\n");
    printf("  3: Error (device not found, permission denied, etc.)\n");
}

static int parse_arguments(int argc, char **argv, cli_options_t *options)
{
    int opt;
    int option_index = 0;

    /* Default options */
    memset(options, 0, sizeof(cli_options_t));
    options->disk_number = -1; // All disks
    options->output_mode = OUTPUT_MODE_SUMMARY;
    options->sector = DEFAULT_SECTOR;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"disk", required_argument, 0, 'd'},
        {"all", no_argument, 0, 'a'},
        {"summary", no_argument, 0, 's'},
        {"full", no_argument, 0, 'f'},
        {"json", no_argument, 0, 'j'},
        {"raw", no_argument, 0, 'r'},
        {"quiet", no_argument, 0, 'q'},
        {"verbose", no_argument, 0, 'V'},
        {"force", no_argument, 0, 'F'},
        {"sector", required_argument, 0, 'S'},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "hvd:asfjqrV", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 'h':
            print_help(argv[0]);
            exit(0);
            break;
        case 'v':
            print_version();
            exit(0);
            break;
        case 'd':
            options->disk_number = atoi(optarg);
            if (options->disk_number < 0 || options->disk_number > 4)
            {
                fprintf(stderr, "Error: Disk number must be 0-4\n");
                return -1;
            }
            break;
        case 'a':
            options->disk_number = -1;
            break;
        case 's':
            options->output_mode = OUTPUT_MODE_SUMMARY;
            break;
        case 'f':
            options->output_mode = OUTPUT_MODE_FULL;
            break;
        case 'j':
            options->output_mode = OUTPUT_MODE_JSON;
            break;
        case 'r':
            options->output_mode = OUTPUT_MODE_RAW;
            break;
        case 'q':
            options->quiet = 1;
            break;
        case 'V':
            options->verbose = 1;
            break;
        case 'F':
            options->force = 1;
            break;
        case 'S':
            options->sector = strtoul(optarg, NULL, 0);
            break;
        default:
            return -1;
        }
    }

    /* Get device path */
    if (optind >= argc)
    {
        fprintf(stderr, "Error: Device path required\n\n");
        print_help(argv[0]);
        return -1;
    }

    strncpy(options->device_path, argv[optind], sizeof(options->device_path) - 1);
    options->device_path[sizeof(options->device_path) - 1] = '\0';

    return 0;
}

static int determine_exit_code(const disk_smart_data_t *disks, int num_disks)
{
    int has_failed = 0;

    (void)num_disks; /* Unused but kept for API consistency */

    for (int i = 0; i < 5; i++)
    {
        if (!disks[i].is_present)
            continue;

        if (disks[i].overall_status == DISK_STATUS_FAILED)
        {
            has_failed = 1;
        }
    }

    if (has_failed)
        return 1;
    return 0;
}

int main(int argc, char **argv)
{
    cli_options_t options;
    int fd;
    uint8_t backup_sector[512];
    disk_smart_data_t disk_data[5];
    int num_disks = 0;
    int exit_code = 0;
    int is_degraded = 0;

    /* Parse command-line arguments */
    if (parse_arguments(argc, argv, &options) != 0)
    {
        return 3;
    }

    /* Validate sector is in safe range */
    if (!is_sector_in_safe_range(options.sector))
    {
        if (!options.quiet)
        {
            fprintf(stderr, "Error: Sector %u is in an unsafe range\n", options.sector);
            fprintf(stderr, "\n");
            fprintf(stderr, "  Unsafe ranges:\n");
            fprintf(stderr, "  - Sectors 0-32, 34-63: MBR, partition table, GPT, boot loaders\n");
            fprintf(stderr, "  - Sector 2048+: Typical first partition location\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "  Safe range: 33 (0x21, original default), 64-2047\n");
            fprintf(stderr, "  Recommended: Use default (1024) or run tests/check_sectors\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "  See SECTOR_USAGE.md for details.\n");
        }
        return 3;
    }

    /* Set verbose mode environment variable for protocol layer */
    if (options.verbose)
    {
        setenv("JMRAIDSTATUS_VERBOSE", "1", 1);
    }

    /* Detect JMicron hardware unless --force is used */
    controller_info_t controller;
    memset(&controller, 0, sizeof(controller));

    if (!options.force)
    {
        if (options.verbose)
        {
            printf("Detecting hardware...\n");
        }

        if (detect_jmicron_hardware(&controller, options.device_path) != 0)
        {
            if (!options.quiet)
            {
                fprintf(stderr, "Error: Could not detect JMicron RAID controller\n");
                fprintf(stderr, "  This tool supports JMicron RAID controllers in USB enclosures or PCIe cards.\n");
                fprintf(stderr, "  Use --force to skip hardware detection and try anyway.\n");
                fprintf(stderr, "\n");
                fprintf(stderr, "  Note: For USB enclosures, detection should work automatically.\n");
                fprintf(stderr, "  If you get this error with a USB enclosure, try --force.\n");
            }
            return 3;
        }

        if (options.verbose)
        {
            if (controller.device_id > 0)
            {
                printf("Detected: %s (%04x:%04x) - %s\n",
                       controller.model, controller.vendor_id, controller.device_id,
                       controller.description);
            }
            else
            {
                printf("Detected: %s - %s\n", controller.model, controller.description);
            }
        }
    }
    else if (options.verbose)
    {
        printf("Skipping hardware detection (--force used).\n");
    }

    /* Show version banner unless quiet mode */
    if (!options.quiet && options.output_mode != OUTPUT_MODE_JSON)
    {
        if (options.output_mode != OUTPUT_MODE_RAW)
        {
            /* Summary and full modes get minimal banner */
        }
    }

    /* Initialize device */
    if (options.verbose)
    {
        printf("Opening device %s...\n", options.device_path);
    }

    int result = jm_init_device(options.device_path, &fd, backup_sector, options.sector);
    if (result != JM_SUCCESS)
    {
        if (!options.quiet)
        {
            fprintf(stderr, "Error: Cannot open %s\n", options.device_path);
            fprintf(stderr, "  %s\n", jm_error_string(result));
            if (result == JM_ERROR_DEVICE_OPEN)
            {
                fprintf(stderr, "  Possible causes:\n");
                fprintf(stderr, "  - Device does not exist\n");
                fprintf(stderr, "  - Permission denied (try sudo)\n");
                fprintf(stderr, "  - Device is busy\n");
            }
        }
        return 3;
    }

    /* Check if sector is empty (all zeros) - safety requirement */
    if (!is_sector_empty(backup_sector, 512))
    {
        if (!options.quiet)
        {
            fprintf(stderr, "Error: Sector %u contains data (not all zeros)\n", options.sector);
            fprintf(stderr, "  The tool requires an empty sector to use as a communication channel.\n");
            fprintf(stderr, "  This sector may contain partition data, RAID metadata, or other critical information.\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "  Safety check failed to prevent potential data corruption.\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "  Solutions:\n");
            fprintf(stderr, "  1. Check your partition layout: sudo fdisk -l %s\n", options.device_path);
            fprintf(stderr, "  2. Use a different sector: --sector XXXX (must be unused)\n");
            fprintf(stderr, "  3. Use tests/check_sectors to find an empty sector\n");
            fprintf(stderr, "\n");
            fprintf(stderr, "  See SECTOR_USAGE.md for details.\n");
        }
        close(fd);
        return 3;
    }

    if (options.verbose)
    {
        printf("Sector %u verified empty (safe to use).\n", options.sector);
    }

    /* Setup signal handlers to ensure cleanup on interruption */
    jm_setup_signal_handlers(fd, options.sector);

    if (options.verbose)
    {
        printf("Signal handlers installed (sector will be restored on Ctrl+C).\n");
    }

    /* Send wakeup sequence */
    if (options.verbose)
    {
        printf("Sending wakeup sequence...\n");
    }

    result = jm_send_wakeup(fd, options.sector);
    if (result != JM_SUCCESS)
    {
        if (!options.quiet)
        {
            fprintf(stderr, "Error: Failed to wake up controller\n");
            fprintf(stderr, "  %s\n", jm_error_string(result));
        }
        jm_cleanup_device(fd, options.sector);
        return 3;
    }

    /* Query disk(s) for SMART data */
    if (options.disk_number >= 0)
    {
        /* Single disk query */
        if (options.verbose)
        {
            printf("Querying disk %d...\n", options.disk_number);
        }

        result = jm_get_disk_smart_data(fd, options.disk_number, NULL,
                                        &disk_data[options.disk_number], options.sector);
        if (result == 0)
        {
            num_disks = 1;
        }
        else
        {
            if (!options.quiet)
            {
                fprintf(stderr, "Error: Failed to read SMART data from disk %d\n",
                        options.disk_number);
            }
            jm_cleanup_device(fd, options.sector);
            return 3;
        }
    }
    else
    {
        /* All disks query */
        if (options.verbose)
        {
            printf("Querying all disks...\n");
        }

        result = jm_get_all_disks_smart_data(fd, disk_data, &num_disks, options.sector, &is_degraded);
        if (result != 0)
        {
            if (!options.quiet)
            {
                fprintf(stderr, "Error: Failed to read SMART data\n");
            }
            jm_cleanup_device(fd, options.sector);
            return 3;
        }

        /* Show degraded RAID warning if detected */
        if (is_degraded && !options.quiet)
        {
            printf("\n");
            printf("=======================================================================\n");
            printf("WARNING: DEGRADED RAID ARRAY DETECTED\n");
            printf("=======================================================================\n");
            printf("Expected 4 disks but found only %d disk%s.\n", num_disks, num_disks == 1 ? "" : "s");
            printf("One or more disks may have failed or been removed.\n");
            printf("RAID array is operating in degraded mode with REDUCED or NO redundancy!\n");
            printf("Replace failed disk(s) immediately to restore redundancy.\n");
            printf("=======================================================================\n\n");
        }
    }

    /* Output results based on mode */
    if (!options.quiet)
    {
        switch (options.output_mode)
        {
        case OUTPUT_MODE_SUMMARY:
            format_summary(options.device_path, disk_data, num_disks,
                           controller.found ? controller.model : NULL);
            break;

        case OUTPUT_MODE_FULL:
            if (options.disk_number >= 0)
            {
                format_full_smart(&disk_data[options.disk_number]);
            }
            else
            {
                /* Show full for all disks */
                for (int i = 0; i < 5; i++)
                {
                    if (disk_data[i].is_present)
                    {
                        format_full_smart(&disk_data[i]);
                        printf("\n");
                    }
                }
            }
            break;

        case OUTPUT_MODE_JSON:
            format_json(options.device_path, disk_data, num_disks);
            break;

        case OUTPUT_MODE_RAW:
            /* This mode would need raw data - not implemented in new architecture
             * Fall back to showing that raw mode requires original code */
            fprintf(stderr, "Raw mode not available in this version.\n");
            fprintf(stderr, "Use --full mode for detailed attribute display.\n");
            break;
        }
    }

    /* Determine exit code based on health status */
    exit_code = determine_exit_code(disk_data, num_disks);

    /* If RAID is degraded and all disks are healthy, return failed exit code */
    if (is_degraded && exit_code == 0)
    {
        exit_code = 1; /* Failed: degraded RAID even though disks are healthy */
    }

    /* Clean up and restore sector */
    if (options.verbose)
    {
        printf("Restoring sector and closing device...\n");
    }

    result = jm_cleanup_device(fd, options.sector);
    if (result != JM_SUCCESS)
    {
        if (!options.quiet)
        {
            fprintf(stderr, "Warning: Failed to restore original sector data\n");
        }
    }

    return exit_code;
}
