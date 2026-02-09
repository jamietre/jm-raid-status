/*
 * disk_health.c - Multi-source SMART data aggregator
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 *
 * Usage: { source1; source2; } | disk-health [OPTIONS]
 * Reads NDJSON from stdin (one JSON object per line)
 * Aggregates and outputs unified report
 */

#define JSMN_STATIC
#include "../jsmn/jsmn.h"
#include "../parsers/common.h"
#include "../smart_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#define MAX_SOURCES 32
#define MAX_LINE_SIZE (1024 * 1024)  /* 1MB per line */

/* Source result from one input line */
typedef struct {
    char backend[32];
    char device[256];
    char controller_model[64];
    char controller_type[32];

    disk_smart_data_t disks[32];
    int num_disks;

    disk_health_status_t overall_status;
    int parse_error;
} source_result_t;

/* Aggregated report */
typedef struct {
    source_result_t sources[MAX_SOURCES];
    int num_sources;

    int total_disks;
    int healthy_disks;
    int failed_disks;
    disk_health_status_t overall_status;

    char timestamp[64];
} aggregated_report_t;

/* CLI options */
typedef struct {
    int output_json;
    int quiet;
    int verbose;
} cli_options_t;

/**
 * Parse one line of disk-health JSON format
 */
static int parse_disk_health_line(const char* line, source_result_t* result) {
    jsmn_parser parser;
    jsmntok_t tokens[10000];

    jsmn_init(&parser);
    int num_tokens = jsmn_parse(&parser, line, strlen(line), tokens, 10000);

    if (num_tokens < 0) {
        fprintf(stderr, "Warning: Failed to parse JSON line (error %d)\n", num_tokens);
        result->parse_error = 1;
        return -1;
    }

    memset(result, 0, sizeof(source_result_t));

    /* Extract basic fields */
    for (int i = 0; i < num_tokens - 1; i++) {
        jsmntok_t* t = &tokens[i];
        if (t->type != JSMN_STRING) continue;

        if (json_token_streq(line, t, "backend")) {
            json_token_tostr(line, &tokens[i + 1], result->backend, sizeof(result->backend));
        }
        else if (json_token_streq(line, t, "device")) {
            json_token_tostr(line, &tokens[i + 1], result->device, sizeof(result->device));
        }
        /* controller.model and controller.type */
        else if (json_token_streq(line, t, "controller") && tokens[i + 1].type == JSMN_OBJECT) {
            int obj_end = tokens[i + 1].end;
            for (int j = i + 2; j < num_tokens && tokens[j].start < obj_end; j++) {
                if (json_token_streq(line, &tokens[j], "model")) {
                    json_token_tostr(line, &tokens[j + 1], result->controller_model,
                                   sizeof(result->controller_model));
                }
                else if (json_token_streq(line, &tokens[j], "type")) {
                    json_token_tostr(line, &tokens[j + 1], result->controller_type,
                                   sizeof(result->controller_type));
                }
            }
        }
        /* disks array */
        else if (json_token_streq(line, t, "disks") && tokens[i + 1].type == JSMN_ARRAY) {
            int array_end = tokens[i + 1].end;
            int k = i + 2;

            while (k < num_tokens && tokens[k].start < array_end &&
                   result->num_disks < 32) {
                if (tokens[k].type == JSMN_OBJECT) {
                    disk_smart_data_t* disk = &result->disks[result->num_disks];
                    memset(disk, 0, sizeof(disk_smart_data_t));
                    disk->is_present = 1;

                    int disk_end = tokens[k].end;
                    for (int m = k + 1; m < num_tokens && tokens[m].start < disk_end; m++) {
                        if (json_token_streq(line, &tokens[m], "disk_number")) {
                            json_token_toint(line, &tokens[m + 1], &disk->disk_number);
                        }
                        else if (json_token_streq(line, &tokens[m], "model")) {
                            json_token_tostr(line, &tokens[m + 1], disk->disk_name,
                                           sizeof(disk->disk_name));
                        }
                        else if (json_token_streq(line, &tokens[m], "serial")) {
                            json_token_tostr(line, &tokens[m + 1], disk->serial_number,
                                           sizeof(disk->serial_number));
                        }
                        else if (json_token_streq(line, &tokens[m], "firmware")) {
                            json_token_tostr(line, &tokens[m + 1], disk->firmware_rev,
                                           sizeof(disk->firmware_rev));
                        }
                        else if (json_token_streq(line, &tokens[m], "size_mb")) {
                            uint64_t size;
                            json_token_touint64(line, &tokens[m + 1], &size);
                            disk->size_mb = size;
                        }
                        else if (json_token_streq(line, &tokens[m], "overall_status")) {
                            char status[16];
                            json_token_tostr(line, &tokens[m + 1], status, sizeof(status));
                            if (strcmp(status, "passed") == 0) {
                                disk->overall_status = DISK_STATUS_PASSED;
                            } else if (strcmp(status, "failed") == 0) {
                                disk->overall_status = DISK_STATUS_FAILED;
                            } else {
                                disk->overall_status = DISK_STATUS_ERROR;
                            }
                        }
                    }

                    result->num_disks++;
                    k++;
                    while (k < num_tokens && tokens[k].start < disk_end) k++;
                } else {
                    k++;
                }
            }
        }
    }

    /* Determine overall status for this source */
    result->overall_status = DISK_STATUS_PASSED;
    for (int i = 0; i < result->num_disks; i++) {
        if (result->disks[i].overall_status == DISK_STATUS_FAILED) {
            result->overall_status = DISK_STATUS_FAILED;
            break;
        }
    }

    return 0;
}

/**
 * Aggregate multiple source results
 */
static void aggregate_sources(aggregated_report_t* report) {
    get_timestamp(report->timestamp, sizeof(report->timestamp));

    report->total_disks = 0;
    report->healthy_disks = 0;
    report->failed_disks = 0;
    report->overall_status = DISK_STATUS_PASSED;

    for (int i = 0; i < report->num_sources; i++) {
        source_result_t* src = &report->sources[i];

        report->total_disks += src->num_disks;

        for (int j = 0; j < src->num_disks; j++) {
            if (src->disks[j].overall_status == DISK_STATUS_PASSED) {
                report->healthy_disks++;
            } else {
                report->failed_disks++;
                report->overall_status = DISK_STATUS_FAILED;
            }
        }
    }
}

/**
 * Output text summary
 */
static void output_summary(const aggregated_report_t* report) {
    printf("Disk Health Report - %s\n\n", report->timestamp);

    printf("Sources: %d\n", report->num_sources);
    for (int i = 0; i < report->num_sources; i++) {
        const source_result_t* src = &report->sources[i];
        const char* status_icon = (src->overall_status == DISK_STATUS_PASSED) ? "✓" : "✗";
        printf("  %s %s %s (%d disk%s)\n",
               status_icon, src->backend, src->device,
               src->num_disks, src->num_disks == 1 ? "" : "s");
    }

    printf("\nOverall Status: %s\n", report->overall_status == DISK_STATUS_PASSED ? "PASSED" : "FAILED");
    printf("  Total Disks: %d\n", report->total_disks);
    printf("  Healthy: %d\n", report->healthy_disks);
    printf("  Failed: %d\n", report->failed_disks);

    printf("\nExit Code: %d (%s)\n",
           report->overall_status == DISK_STATUS_PASSED ? 0 : 1,
           report->overall_status == DISK_STATUS_PASSED ? "all healthy" : "failures detected");
}

/**
 * Output aggregated JSON
 */
static void output_json(const aggregated_report_t* report) {
    printf("{\n");
    printf("  \"version\": \"2.0\",\n");
    printf("  \"timestamp\": \"%s\",\n", report->timestamp);

    printf("  \"sources\": [\n");
    for (int i = 0; i < report->num_sources; i++) {
        const source_result_t* src = &report->sources[i];
        if (i > 0) printf(",\n");

        printf("    {\n");
        printf("      \"backend\": \"%s\",\n", src->backend);
        printf("      \"device\": \"%s\",\n", src->device);
        printf("      \"controller\": {\n");
        printf("        \"model\": \"%s\",\n", src->controller_model);
        printf("        \"type\": \"%s\"\n", src->controller_type);
        printf("      },\n");
        printf("      \"num_disks\": %d,\n", src->num_disks);
        printf("      \"status\": \"%s\"\n",
               src->overall_status == DISK_STATUS_PASSED ? "passed" : "failed");
        printf("    }");
    }
    printf("\n  ],\n");

    printf("  \"summary\": {\n");
    printf("    \"total_disks\": %d,\n", report->total_disks);
    printf("    \"healthy_disks\": %d,\n", report->healthy_disks);
    printf("    \"failed_disks\": %d,\n", report->failed_disks);
    printf("    \"overall_status\": \"%s\"\n",
           report->overall_status == DISK_STATUS_PASSED ? "passed" : "failed");
    printf("  }\n");
    printf("}\n");
}

/**
 * Parse command-line arguments
 */
static void parse_arguments(int argc, char** argv, cli_options_t* options) {
    memset(options, 0, sizeof(cli_options_t));

    static struct option long_options[] = {
        {"json", no_argument, 0, 'j'},
        {"quiet", no_argument, 0, 'q'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "jqvh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'j':
                options->output_json = 1;
                break;
            case 'q':
                options->quiet = 1;
                break;
            case 'v':
                options->verbose = 1;
                break;
            case 'h':
                printf("Usage: disk-health [OPTIONS]\n\n");
                printf("Aggregate SMART data from multiple sources\n\n");
                printf("Options:\n");
                printf("  -j, --json     Output aggregated JSON\n");
                printf("  -q, --quiet    Minimal output (exit code only)\n");
                printf("  -v, --verbose  Verbose output\n");
                printf("  -h, --help     Show this help\n\n");
                printf("Input: NDJSON from stdin (one JSON object per line)\n");
                printf("Output: Text summary or JSON aggregate\n");
                exit(0);
            default:
                exit(1);
        }
    }
}

int main(int argc, char** argv) {
    cli_options_t options;
    parse_arguments(argc, argv, &options);

    aggregated_report_t report;
    memset(&report, 0, sizeof(report));

    /* Read NDJSON from stdin (one JSON object per line) */
    char* line = malloc(MAX_LINE_SIZE);
    if (!line) {
        fprintf(stderr, "Error: Out of memory\n");
        return 3;
    }

    while (fgets(line, MAX_LINE_SIZE, stdin)) {
        /* Skip empty lines */
        if (line[0] == '\n' || line[0] == '\0') {
            continue;
        }

        if (report.num_sources >= MAX_SOURCES) {
            fprintf(stderr, "Warning: Maximum sources (%d) exceeded, ignoring rest\n", MAX_SOURCES);
            break;
        }

        if (options.verbose) {
            fprintf(stderr, "Parsing source %d...\n", report.num_sources + 1);
        }

        source_result_t* result = &report.sources[report.num_sources];
        if (parse_disk_health_line(line, result) == 0) {
            report.num_sources++;
        }
    }

    free(line);

    if (report.num_sources == 0) {
        if (!options.quiet) {
            fprintf(stderr, "Error: No valid sources found on stdin\n");
            fprintf(stderr, "Expected NDJSON input (one JSON object per line)\n");
        }
        return 3;
    }

    /* Aggregate results */
    aggregate_sources(&report);

    /* Output based on mode */
    if (!options.quiet) {
        if (options.output_json) {
            output_json(&report);
        } else {
            output_summary(&report);
        }
    }

    /* Exit code based on overall health */
    return (report.overall_status == DISK_STATUS_PASSED) ? 0 : 1;
}
