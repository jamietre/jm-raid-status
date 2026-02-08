/**
 * config.c - Configuration file handling for SMART thresholds
 *
 * Simple JSON parser for configuration files.
 * No external dependencies - minimal parser for our specific format.
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "config.h"

#define MAX_CONFIG_SIZE 65536  /* 64KB max config file */
#define MAX_ATTRIBUTES 32      /* Max custom attribute thresholds */

void config_init_default(smart_config_t* config) {
    memset(config, 0, sizeof(smart_config_t));

    /* Default: no custom thresholds, use manufacturer only */
    config->use_manufacturer_thresholds = 1;
    config->has_temp_critical = 0;
    config->attributes = NULL;
    config->num_attributes = 0;
}

void config_free(smart_config_t* config) {
    if (config->attributes) {
        free(config->attributes);
        config->attributes = NULL;
    }
    config->num_attributes = 0;
}

int config_write_default(const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Error: Could not write config file: %s\n", path);
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"use_manufacturer_thresholds\": true,\n");
    fprintf(f, "  \"temperature\": {\n");
    fprintf(f, "    \"critical\": 60\n");
    fprintf(f, "  },\n");
    fprintf(f, "  \"attributes\": {\n");
    fprintf(f, "    \"0x05\": {\n");
    fprintf(f, "      \"name\": \"Reallocated Sector Count\",\n");
    fprintf(f, "      \"raw_critical\": 0\n");
    fprintf(f, "    },\n");
    fprintf(f, "    \"0xC5\": {\n");
    fprintf(f, "      \"name\": \"Current Pending Sector Count\",\n");
    fprintf(f, "      \"raw_critical\": 0\n");
    fprintf(f, "    },\n");
    fprintf(f, "    \"0xC6\": {\n");
    fprintf(f, "      \"name\": \"Offline Uncorrectable Sector Count\",\n");
    fprintf(f, "      \"raw_critical\": 0\n");
    fprintf(f, "    },\n");
    fprintf(f, "    \"0x0A\": {\n");
    fprintf(f, "      \"name\": \"Spin Retry Count\",\n");
    fprintf(f, "      \"raw_critical\": 0\n");
    fprintf(f, "    }\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);
    printf("Default configuration written to: %s\n", path);
    printf("\nThis file defines SMART attribute thresholds.\n");
    printf("Edit the values to customize when attributes are considered failed.\n");
    printf("\nuse_manufacturer_thresholds: If true, also check drive's built-in thresholds\n");
    printf("temperature.critical: Temperature in °C to consider critical\n");
    printf("attributes.0xNN.raw_critical: Fail if raw value exceeds this\n");

    return 0;
}

/* Simple JSON token types */
typedef enum {
    TOK_LBRACE, TOK_RBRACE, TOK_LBRACKET, TOK_RBRACKET,
    TOK_COLON, TOK_COMMA, TOK_STRING, TOK_NUMBER,
    TOK_TRUE, TOK_FALSE, TOK_NULL, TOK_EOF, TOK_ERROR
} token_type_t;

typedef struct {
    token_type_t type;
    char string_value[256];
    long number_value;
    int bool_value;
} token_t;

/* Simple tokenizer state */
typedef struct {
    const char* input;
    size_t pos;
    size_t len;
} tokenizer_t;

static void skip_whitespace(tokenizer_t* t) {
    while (t->pos < t->len && isspace(t->input[t->pos])) {
        t->pos++;
    }
}

static int get_token(tokenizer_t* t, token_t* tok) {
    skip_whitespace(t);

    if (t->pos >= t->len) {
        tok->type = TOK_EOF;
        return 0;
    }

    char c = t->input[t->pos];

    /* Single-character tokens */
    if (c == '{') { tok->type = TOK_LBRACE; t->pos++; return 0; }
    if (c == '}') { tok->type = TOK_RBRACE; t->pos++; return 0; }
    if (c == '[') { tok->type = TOK_LBRACKET; t->pos++; return 0; }
    if (c == ']') { tok->type = TOK_RBRACKET; t->pos++; return 0; }
    if (c == ':') { tok->type = TOK_COLON; t->pos++; return 0; }
    if (c == ',') { tok->type = TOK_COMMA; t->pos++; return 0; }

    /* String */
    if (c == '"') {
        t->pos++;
        size_t i = 0;
        while (t->pos < t->len && t->input[t->pos] != '"' && i < 255) {
            tok->string_value[i++] = t->input[t->pos++];
        }
        tok->string_value[i] = '\0';
        if (t->pos < t->len && t->input[t->pos] == '"') {
            t->pos++;
        }
        tok->type = TOK_STRING;
        return 0;
    }

    /* Number (including hex like 0x05) */
    if (isdigit(c) || c == '-') {
        char num_buf[32];
        size_t i = 0;
        while (t->pos < t->len && (isxdigit(t->input[t->pos]) ||
               t->input[t->pos] == 'x' || t->input[t->pos] == 'X' ||
               t->input[t->pos] == '-') && i < 31) {
            num_buf[i++] = t->input[t->pos++];
        }
        num_buf[i] = '\0';

        /* Parse as hex if 0x prefix, otherwise decimal */
        if (strncmp(num_buf, "0x", 2) == 0 || strncmp(num_buf, "0X", 2) == 0) {
            tok->number_value = strtol(num_buf, NULL, 16);
        } else {
            tok->number_value = strtol(num_buf, NULL, 10);
        }
        tok->type = TOK_NUMBER;
        return 0;
    }

    /* Boolean and null */
    if (strncmp(t->input + t->pos, "true", 4) == 0) {
        tok->type = TOK_TRUE;
        tok->bool_value = 1;
        t->pos += 4;
        return 0;
    }
    if (strncmp(t->input + t->pos, "false", 5) == 0) {
        tok->type = TOK_FALSE;
        tok->bool_value = 0;
        t->pos += 5;
        return 0;
    }
    if (strncmp(t->input + t->pos, "null", 4) == 0) {
        tok->type = TOK_NULL;
        t->pos += 4;
        return 0;
    }

    tok->type = TOK_ERROR;
    return -1;
}

int config_load(const char* path, smart_config_t* config) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open config file: %s\n", path);
        return -1;
    }

    /* Read entire file */
    char* buffer = malloc(MAX_CONFIG_SIZE);
    if (!buffer) {
        fclose(f);
        return -1;
    }

    size_t bytes_read = fread(buffer, 1, MAX_CONFIG_SIZE - 1, f);
    buffer[bytes_read] = '\0';
    fclose(f);

    /* Initialize config with defaults */
    config_init_default(config);

    /* Tokenize */
    tokenizer_t tokenizer = { buffer, 0, bytes_read };
    token_t tok;

    /* Expect opening brace */
    if (get_token(&tokenizer, &tok) != 0 || tok.type != TOK_LBRACE) {
        fprintf(stderr, "Error: Config file must start with '{'\n");
        free(buffer);
        return -1;
    }

    /* Temporary storage for attributes */
    attribute_threshold_t temp_attrs[MAX_ATTRIBUTES];
    int attr_count = 0;

    /* Parse key-value pairs */
    while (1) {
        if (get_token(&tokenizer, &tok) != 0) break;
        if (tok.type == TOK_RBRACE) break;  /* End of object */
        if (tok.type == TOK_COMMA) continue;

        if (tok.type != TOK_STRING) {
            fprintf(stderr, "Error: Expected property name (string)\n");
            free(buffer);
            return -1;
        }

        char key[256];
        strncpy(key, tok.string_value, sizeof(key) - 1);

        /* Expect colon */
        if (get_token(&tokenizer, &tok) != 0 || tok.type != TOK_COLON) {
            fprintf(stderr, "Error: Expected ':' after property name\n");
            free(buffer);
            return -1;
        }

        /* Get value */
        if (get_token(&tokenizer, &tok) != 0) {
            fprintf(stderr, "Error: Expected value\n");
            free(buffer);
            return -1;
        }

        /* Parse known properties */
        if (strcmp(key, "use_manufacturer_thresholds") == 0) {
            if (tok.type == TOK_TRUE || tok.type == TOK_FALSE) {
                config->use_manufacturer_thresholds = tok.bool_value;
            }
        }
        else if (strcmp(key, "temperature") == 0) {
            if (tok.type != TOK_LBRACE) continue;

            /* Parse temperature object */
            while (1) {
                if (get_token(&tokenizer, &tok) != 0) break;
                if (tok.type == TOK_RBRACE) break;
                if (tok.type == TOK_COMMA) continue;
                if (tok.type != TOK_STRING) break;

                if (strcmp(tok.string_value, "critical") == 0) {
                    if (get_token(&tokenizer, &tok) == 0 && tok.type == TOK_COLON) {
                        if (get_token(&tokenizer, &tok) == 0 && tok.type == TOK_NUMBER) {
                            config->has_temp_critical = 1;
                            config->temp_critical = (uint8_t)tok.number_value;
                        }
                    }
                }
            }
        }
        else if (strcmp(key, "attributes") == 0) {
            if (tok.type != TOK_LBRACE) continue;

            /* Parse attributes object */
            while (1) {
                if (get_token(&tokenizer, &tok) != 0) break;
                if (tok.type == TOK_RBRACE) break;
                if (tok.type == TOK_COMMA) continue;
                if (tok.type != TOK_STRING) break;

                /* Attribute ID (e.g., "0x05") */
                uint8_t attr_id = (uint8_t)strtol(tok.string_value, NULL, 16);

                if (get_token(&tokenizer, &tok) != 0 || tok.type != TOK_COLON) break;
                if (get_token(&tokenizer, &tok) != 0 || tok.type != TOK_LBRACE) break;

                /* Parse attribute properties */
                attribute_threshold_t attr_thresh;
                memset(&attr_thresh, 0, sizeof(attr_thresh));
                attr_thresh.id = attr_id;

                while (1) {
                    if (get_token(&tokenizer, &tok) != 0) break;
                    if (tok.type == TOK_RBRACE) break;
                    if (tok.type == TOK_COMMA) continue;
                    if (tok.type != TOK_STRING) break;

                    if (strcmp(tok.string_value, "raw_critical") == 0) {
                        if (get_token(&tokenizer, &tok) == 0 && tok.type == TOK_COLON) {
                            if (get_token(&tokenizer, &tok) == 0 && tok.type == TOK_NUMBER) {
                                attr_thresh.has_raw_critical = 1;
                                attr_thresh.raw_critical = (uint64_t)tok.number_value;
                            }
                        }
                    } else {
                        /* Skip unknown property value */
                        if (get_token(&tokenizer, &tok) == 0 && tok.type == TOK_COLON) {
                            get_token(&tokenizer, &tok);  /* consume value */
                        }
                    }
                }

                /* Store attribute threshold */
                if (attr_count < MAX_ATTRIBUTES) {
                    temp_attrs[attr_count++] = attr_thresh;
                }
            }
        }
        else {
            /* Skip unknown top-level property */
            /* Value was already consumed, continue */
        }
    }

    /* Copy attributes to config */
    if (attr_count > 0) {
        config->attributes = malloc(sizeof(attribute_threshold_t) * attr_count);
        if (config->attributes) {
            memcpy(config->attributes, temp_attrs, sizeof(attribute_threshold_t) * attr_count);
            config->num_attributes = attr_count;
        }
    }

    free(buffer);
    return 0;
}
