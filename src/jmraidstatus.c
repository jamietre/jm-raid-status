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
#include <unistd.h>
#include "jm_protocol.h"
#include "jm_commands.h"
#include "smart_parser.h"
#include "output_formatter.h"
#include "config.h"
#include "hardware_detect.h"

#define VERSION "1.0"
#define DEFAULT_SECTOR 33 /* Original sector from jmraidcon - most compatible */

/* Command-line options structure */
typedef struct
{
    char device_path[256];
    int disk_number; // -1 = all disks
    output_mode_t output_mode;
    int verbose;
    int quiet;
    int force; // Skip hardware detection
    int dump_raw; // Dump raw protocol data
    uint32_t sector;
    int expected_array_size; // Expected number of disks (0 = not specified)
    char config_path[256]; // Path to config file (empty = use defaults)
    char write_default_config_path[256]; // If set, write default config and exit
} cli_options_t;

/* Hardware detection functions now in hardware_detect.c */

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

/* Hardware detection functions moved to hardware_detect.c */

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
    printf("  --json-only             JSON output only (no extra messages, implies --quiet)\n");
    printf("  -r, --raw               Dump raw protocol data to stderr (debug mode)\n");
    printf("  -q, --quiet             Minimal output (exit code only)\n");
    printf("  --verbose               Verbose output with debug info\n");
    printf("  --force                 Skip hardware detection (use with caution)\n");
    printf("  --sector SECTOR         Use specific sector number (default: %u)\n", DEFAULT_SECTOR);
    printf("  --array-size N          Expected number of disks (fail if mismatch detected)\n");
    printf("  --config PATH           Load custom SMART threshold configuration\n");
    printf("  --write-default-config PATH  Write default config file and exit\n");
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
    options->expected_array_size = 0; // Not specified
    options->config_path[0] = '\0'; // No config file
    options->write_default_config_path[0] = '\0'; // Not writing config

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"disk", required_argument, 0, 'd'},
        {"all", no_argument, 0, 'a'},
        {"summary", no_argument, 0, 's'},
        {"full", no_argument, 0, 'f'},
        {"json", no_argument, 0, 'j'},
        {"json-only", no_argument, 0, 'J'},
        {"raw", no_argument, 0, 'r'},
        {"quiet", no_argument, 0, 'q'},
        {"verbose", no_argument, 0, 'V'},
        {"force", no_argument, 0, 'F'},
        {"sector", required_argument, 0, 'S'},
        {"array-size", required_argument, 0, 'A'},
        {"config", required_argument, 0, 'C'},
        {"write-default-config", required_argument, 0, 'W'},
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
        case 'J':
            /* --json-only: JSON output with no extra messages */
            options->output_mode = OUTPUT_MODE_JSON;
            options->quiet = 1;
            break;
        case 'r':
            options->dump_raw = 1;
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
        case 'A':
            options->expected_array_size = atoi(optarg);
            if (options->expected_array_size < 1 || options->expected_array_size > 5)
            {
                fprintf(stderr, "Error: Array size must be 1-5\n");
                return -1;
            }
            break;
        case 'C':
            strncpy(options->config_path, optarg, sizeof(options->config_path) - 1);
            options->config_path[sizeof(options->config_path) - 1] = '\0';
            break;
        case 'W':
            strncpy(options->write_default_config_path, optarg, sizeof(options->write_default_config_path) - 1);
            options->write_default_config_path[sizeof(options->write_default_config_path) - 1] = '\0';
            break;
        default:
            return -1;
        }
    }

    /* Get device path (not required for --write-default-config) */
    if (optind >= argc)
    {
        if (options->write_default_config_path[0] == '\0')
        {
            fprintf(stderr, "Error: Device path required\n\n");
            print_help(argv[0]);
            return -1;
        }
        /* --write-default-config doesn't need device path */
        options->device_path[0] = '\0';
    }
    else
    {
        strncpy(options->device_path, argv[optind], sizeof(options->device_path) - 1);
        options->device_path[sizeof(options->device_path) - 1] = '\0';
    }

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
    int present_disks = 0;  /* From controller bitmask */

    /* Parse command-line arguments */
    if (parse_arguments(argc, argv, &options) != 0)
    {
        return 3;
    }

    /* Handle --write-default-config (write config and exit) */
    if (options.write_default_config_path[0] != '\0')
    {
        return (config_write_default(options.write_default_config_path) == 0) ? 0 : 3;
    }

    /* Load config if specified, otherwise use defaults */
    smart_config_t config;
    if (options.config_path[0] != '\0')
    {
        if (config_load(options.config_path, &config) != 0)
        {
            if (!options.quiet)
            {
                fprintf(stderr, "Error: Failed to load config from %s\n", options.config_path);
            }
            return 3;
        }
        if (options.verbose)
        {
            printf("Loaded config from: %s\n", options.config_path);
        }
    }
    else
    {
        /* No config file specified - use defaults */
        config_init_default(&config);
    }

    /* Set global config for SMART assessment */
    smart_set_config(&config);

    /* Set global runtime context for JM commands */
    jm_runtime_context_t runtime_ctx = {
        .verbose = options.verbose,
        .dump_raw = options.dump_raw,
        .sector = options.sector,
        .expected_array_size = options.expected_array_size
    };
    jm_set_context(&runtime_ctx);

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

    /* Show version banner unless quiet mode or JSON mode */
    if (!options.quiet && options.output_mode != OUTPUT_MODE_JSON)
    {
        /* Summary and full modes get minimal banner */
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
                                        &disk_data[options.disk_number]);
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

        result = jm_get_all_disks_smart_data(fd, disk_data, &num_disks, &is_degraded, &present_disks);
        if (result != 0)
        {
            if (!options.quiet)
            {
                fprintf(stderr, "Error: Failed to read SMART data\n");
            }
            jm_cleanup_device(fd, options.sector);
            return 3;
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
            format_json(options.device_path, disk_data, num_disks,
                        options.expected_array_size, present_disks, is_degraded,
                        controller.found ? controller.model : NULL);
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

    /* Show RAID array size warnings at end (after SMART data)
     * Skip in JSON mode - warnings are included in JSON output */
    if (options.expected_array_size > 0 && present_disks > 0 && !options.quiet &&
        options.output_mode != OUTPUT_MODE_JSON)
    {
        if (is_degraded)
        {
            /* Degraded: fewer disks than expected */
            printf("\n");
            printf("=======================================================================\n");
            printf("WARNING: DEGRADED RAID ARRAY DETECTED\n");
            printf("=======================================================================\n");
            printf("Expected %d disk%s but found only %d disk%s.\n",
                   options.expected_array_size, options.expected_array_size == 1 ? "" : "s",
                   present_disks, present_disks == 1 ? "" : "s");
            printf("One or more disks may have failed or been removed.\n");
            printf("RAID array is operating in degraded mode with REDUCED or NO redundancy!\n");
            printf("Replace failed disk(s) immediately to restore redundancy.\n");
            printf("=======================================================================\n\n");
        }
        else if (present_disks > options.expected_array_size)
        {
            /* More disks than expected */
            printf("\n");
            printf("=======================================================================\n");
            printf("WARNING: MORE DISKS THAN EXPECTED\n");
            printf("=======================================================================\n");
            printf("Expected %d disk%s but found %d disk%s.\n",
                   options.expected_array_size, options.expected_array_size == 1 ? "" : "s",
                   present_disks, present_disks == 1 ? "" : "s");
            printf("This may indicate:\n");
            printf("  - Incorrect --array-size specified (check your array configuration)\n");
            printf("  - Extra disk added to array\n");
            printf("  - Array configuration changed\n");
            printf("=======================================================================\n\n");
        }
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

    /* Free config resources */
    config_free(&config);

    return exit_code;
}
