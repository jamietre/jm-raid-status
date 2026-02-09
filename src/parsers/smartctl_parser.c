/*
 * smartctl_parser.c - Convert smartctl JSON to disk-health format
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 *
 * Usage: smartctl --json=c /dev/sda | smartctl-parser
 * Output: Single line of compact JSON in disk-health format
 */

#define JSMN_STATIC
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

    /* Simple linear scan - in jsmn, keys and values are sequential tokens */
    for (int i = 0; i < num_tokens - 1; i++) {
        jsmntok_t* t = &tokens[i];

        if (t->type != JSMN_STRING) continue;

        /* Top-level fields */
        if (json_token_streq(json, t, "model_name")) {
            json_token_tostr(json, &tokens[i + 1], data->model, sizeof(data->model));
        }
        else if (json_token_streq(json, t, "serial_number")) {
            json_token_tostr(json, &tokens[i + 1], data->serial, sizeof(data->serial));
        }
        else if (json_token_streq(json, t, "firmware_version")) {
            json_token_tostr(json, &tokens[i + 1], data->firmware, sizeof(data->firmware));
        }
        /* device.name */
        else if (json_token_streq(json, t, "device") && tokens[i + 1].type == JSMN_OBJECT) {
            /* Look for "name" key within this object */
            int obj_end = tokens[i + 1].end;
            for (int j = i + 2; j < num_tokens && tokens[j].start < obj_end; j++) {
                if (json_token_streq(json, &tokens[j], "name")) {
                    json_token_tostr(json, &tokens[j + 1], data->device, sizeof(data->device));
                    break;
                }
            }
        }
        /* user_capacity.bytes */
        else if (json_token_streq(json, t, "user_capacity") && tokens[i + 1].type == JSMN_OBJECT) {
            int obj_end = tokens[i + 1].end;
            for (int j = i + 2; j < num_tokens && tokens[j].start < obj_end; j++) {
                if (json_token_streq(json, &tokens[j], "bytes")) {
                    json_token_touint64(json, &tokens[j + 1], &data->size_bytes);
                    break;
                }
            }
        }
        /* temperature.current */
        else if (json_token_streq(json, t, "temperature") && tokens[i + 1].type == JSMN_OBJECT) {
            int obj_end = tokens[i + 1].end;
            for (int j = i + 2; j < num_tokens && tokens[j].start < obj_end; j++) {
                if (json_token_streq(json, &tokens[j], "current")) {
                    json_token_toint(json, &tokens[j + 1], &data->temperature);
                    data->has_temperature = 1;
                    break;
                }
            }
        }
        /* ata_smart_attributes.table array */
        else if (json_token_streq(json, t, "ata_smart_attributes") && tokens[i + 1].type == JSMN_OBJECT) {
            int obj_end = tokens[i + 1].end;
            for (int j = i + 2; j < num_tokens && tokens[j].start < obj_end; j++) {
                if (json_token_streq(json, &tokens[j], "table") && tokens[j + 1].type == JSMN_ARRAY) {
                    /* Parse array of SMART attributes */
                    int array_end = tokens[j + 1].end;
                    int k = j + 2;  /* First element after array token */

                    while (k < num_tokens && tokens[k].start < array_end &&
                           data->num_attributes < MAX_SMART_ATTRIBUTES) {
                        if (tokens[k].type == JSMN_OBJECT) {
                            parsed_smart_attribute_t* attr = &data->attributes[data->num_attributes];
                            memset(attr, 0, sizeof(parsed_smart_attribute_t));

                            int attr_end = tokens[k].end;
                            /* Parse this attribute object */
                            for (int m = k + 1; m < num_tokens && tokens[m].start < attr_end; m++) {
                                if (json_token_streq(json, &tokens[m], "id")) {
                                    int id;
                                    json_token_toint(json, &tokens[m + 1], &id);
                                    attr->id = (uint8_t)id;
                                }
                                else if (json_token_streq(json, &tokens[m], "value")) {
                                    int val;
                                    json_token_toint(json, &tokens[m + 1], &val);
                                    attr->current_value = (uint8_t)val;
                                }
                                else if (json_token_streq(json, &tokens[m], "worst")) {
                                    int val;
                                    json_token_toint(json, &tokens[m + 1], &val);
                                    attr->worst_value = (uint8_t)val;
                                }
                                else if (json_token_streq(json, &tokens[m], "thresh")) {
                                    int val;
                                    json_token_toint(json, &tokens[m + 1], &val);
                                    attr->threshold = (uint8_t)val;
                                }
                                else if (json_token_streq(json, &tokens[m], "raw") &&
                                         tokens[m + 1].type == JSMN_OBJECT) {
                                    /* raw.value */
                                    int raw_end = tokens[m + 1].end;
                                    for (int n = m + 2; n < num_tokens && tokens[n].start < raw_end; n++) {
                                        if (json_token_streq(json, &tokens[n], "value")) {
                                            json_token_touint64(json, &tokens[n + 1], &attr->raw_value);
                                            break;
                                        }
                                    }
                                }
                            }

                            /* Get attribute definition for name and criticality */
                            const smart_attribute_def_t* def = get_attribute_definition(attr->id);
                            if (def) {
                                attr->name = def->name;
                                attr->is_critical = def->is_critical;
                            }

                            data->num_attributes++;

                            /* Skip to end of this object */
                            k++;
                            while (k < num_tokens && tokens[k].start < attr_end) k++;
                        } else {
                            k++;
                        }
                    }
                    break;
                }
            }
        }
    }

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
