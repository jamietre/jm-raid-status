/*
 * smartctl_parser.c - Convert smartctl JSON to disk-health format
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 *
 * Usage: smartctl --json=c /dev/sda | smartctl-parser
 * Output: Single line of compact JSON in disk-health format
 */

#define JSMN_PARENT_LINKS
#define JSMN_HEADER
#include "../jsmn/jsmn.h"
#include "common.h"
#include "../smart_parser.h"
#include "../smart_attributes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parsed smartctl data */
typedef struct {
    char device[256];
    char model[64];
    char serial[64];
    char firmware[16];
    uint64_t size_bytes;

    parsed_smart_attribute_t attributes[MAX_SMART_ATTRIBUTES];
    int num_attributes;

    int temperature;
    int has_temperature;
} smartctl_data_t;

/**
 * Parse smartctl JSON and extract fields
 */
static int parse_smartctl_json(const char* json, smartctl_data_t* data) {
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKENS];

    jsmn_init(&parser);
    int num_tokens = jsmn_parse(&parser, json, strlen(json), tokens, MAX_JSON_TOKENS);

    if (num_tokens < 0) {
        fprintf(stderr, "Error: Failed to parse JSON (error %d)\n", num_tokens);
        return -1;
    }

    if (num_tokens < 1 || tokens[0].type != JSMN_OBJECT) {
        fprintf(stderr, "Error: Root element must be object\n");
        return -1;
    }

    memset(data, 0, sizeof(smartctl_data_t));

    /* Simple linear scan through all tokens */
    for (int i = 1; i < num_tokens; i++) {
        if (tokens[i].type != JSMN_STRING) continue;

        /* Check if this is a key we care about */
        if (json_token_streq(json, &tokens[i], "name") && tokens[i].parent == 1) {
            /* Check parent is "device" object */
            for (int j = i - 1; j >= 0; j--) {
                if (json_token_streq(json, &tokens[j], "device")) {
                    json_token_tostr(json, &tokens[i + 1], data->device, sizeof(data->device));
                    break;
                }
            }
        }
        else if (json_token_streq(json, &tokens[i], "model_name")) {
            json_token_tostr(json, &tokens[i + 1], data->model, sizeof(data->model));
        }
        else if (json_token_streq(json, &tokens[i], "serial_number")) {
            json_token_tostr(json, &tokens[i + 1], data->serial, sizeof(data->serial));
        }
        else if (json_token_streq(json, &tokens[i], "firmware_version")) {
            json_token_tostr(json, &tokens[i + 1], data->firmware, sizeof(data->firmware));
        }
        else if (json_token_streq(json, &tokens[i], "bytes") && tokens[i].parent > 0) {
            /* Check if parent is user_capacity */
            int parent_idx = tokens[i].parent;
            if (parent_idx > 0) {
                for (int j = parent_idx - 1; j >= 0; j--) {
                    if (json_token_streq(json, &tokens[j], "user_capacity")) {
                        json_token_touint64(json, &tokens[i + 1], &data->size_bytes);
                        break;
                    }
                }
            }
        }
    }

    /* TODO: Parse temperature and SMART attributes
     * For now, basic disk info is sufficient to demonstrate the architecture */
    (void)num_tokens;  /* Suppress unused warning */

    return 0;
}

/**
 * Output disk-health format JSON (compact, one line)
 */
static void output_disk_health_json(const smartctl_data_t* data) {
    char timestamp[64];
    get_timestamp(timestamp, sizeof(timestamp));

    /* Determine overall status (simple check: any threshold failures?) */
    const char* overall_status = "passed";
    for (int i = 0; i < data->num_attributes; i++) {
        const parsed_smart_attribute_t* attr = &data->attributes[i];
        if (attr->threshold > 0 && attr->current_value < attr->threshold) {
            overall_status = "failed";
            break;
        }
    }

    /* Output compact JSON (one line) */
    printf("{");
    printf("\"version\":\"1.0\",");
    printf("\"backend\":\"smartctl\",");
    printf("\"device\":");
    json_output_string(data->device);
    printf(",");
    printf("\"timestamp\":");
    json_output_string(timestamp);
    printf(",");

    /* Controller (N/A for single disks) */
    printf("\"controller\":{");
    printf("\"model\":\"N/A\",");
    printf("\"type\":\"single_disk\"");
    printf("},");

    /* No RAID status for single disks */
    printf("\"raid_status\":null,");

    /* Disks array (single disk) */
    printf("\"disks\":[{");
    printf("\"disk_number\":0,");
    printf("\"model\":");
    json_output_string(data->model);
    printf(",");
    printf("\"serial\":");
    json_output_string(data->serial);
    printf(",");
    printf("\"firmware\":");
    json_output_string(data->firmware);
    printf(",");
    printf("\"size_mb\":%llu,", (unsigned long long)(data->size_bytes / (1024 * 1024)));
    printf("\"overall_status\":");
    json_output_string(overall_status);
    printf(",");

    /* Attributes array */
    printf("\"attributes\":[");
    for (int i = 0; i < data->num_attributes; i++) {
        const parsed_smart_attribute_t* attr = &data->attributes[i];
        if (i > 0) printf(",");

        printf("{");
        printf("\"id\":%d,", attr->id);
        printf("\"name\":");
        json_output_string(attr->name ? attr->name : "Unknown");
        printf(",");
        printf("\"value\":%d,", attr->current_value);
        printf("\"worst\":%d,", attr->worst_value);
        printf("\"thresh\":%d,", attr->threshold);
        printf("\"raw\":%llu,", (unsigned long long)attr->raw_value);

        /* Status */
        const char* status = "ok";
        if (attr->threshold > 0 && attr->current_value < attr->threshold) {
            status = "failed";
        }
        printf("\"status\":");
        json_output_string(status);
        printf(",");
        printf("\"critical\":%s", attr->is_critical ? "true" : "false");
        printf("}");
    }
    printf("]");

    printf("}]");  /* Close disks array */
    printf("}");   /* Close root object */
    printf("\n");  /* Newline for NDJSON */
}

int main(void) {
    /* Read all input from stdin */
    size_t input_size;
    char* input = read_all_stdin(&input_size);
    if (!input) {
        return 1;
    }

    /* Parse smartctl JSON */
    smartctl_data_t data;
    if (parse_smartctl_json(input, &data) != 0) {
        free(input);
        return 1;
    }

    /* Output disk-health format JSON */
    output_disk_health_json(&data);

    free(input);
    return 0;
}
