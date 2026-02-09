/*
 * common.c - Common parser utilities implementation
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

char* read_all_stdin(size_t* size) {
    size_t capacity = 4096;
    size_t used = 0;
    char* buffer = malloc(capacity);

    if (!buffer) {
        fprintf(stderr, "Error: Out of memory\n");
        return NULL;
    }

    while (1) {
        size_t space = capacity - used;
        if (space < 1024) {
            capacity *= 2;
            if (capacity > MAX_JSON_INPUT_SIZE) {
                fprintf(stderr, "Error: Input too large (>%d bytes)\n", MAX_JSON_INPUT_SIZE);
                free(buffer);
                return NULL;
            }
            char* new_buffer = realloc(buffer, capacity);
            if (!new_buffer) {
                fprintf(stderr, "Error: Out of memory\n");
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
            space = capacity - used;
        }

        size_t nread = fread(buffer + used, 1, space, stdin);
        if (nread == 0) {
            if (feof(stdin)) {
                break;
            }
            if (ferror(stdin)) {
                fprintf(stderr, "Error reading stdin: %s\n", strerror(errno));
                free(buffer);
                return NULL;
            }
        }
        used += nread;
    }

    buffer[used] = '\0';
    if (size) {
        *size = used;
    }
    return buffer;
}

int json_token_streq(const char* json, jsmntok_t* tok, const char* s) {
    if (tok->type != JSMN_STRING) {
        return 0;
    }
    int len = tok->end - tok->start;
    return (int)strlen(s) == len && strncmp(json + tok->start, s, len) == 0;
}

int json_token_tostr(const char* json, jsmntok_t* tok, char* buf, size_t bufsize) {
    if (tok->type != JSMN_STRING && tok->type != JSMN_PRIMITIVE) {
        return -1;
    }

    int len = tok->end - tok->start;
    if (len >= (int)bufsize) {
        len = bufsize - 1;
    }

    memcpy(buf, json + tok->start, len);
    buf[len] = '\0';
    return 0;
}

int json_token_toint(const char* json, jsmntok_t* tok, int* value) {
    if (tok->type != JSMN_PRIMITIVE) {
        return -1;
    }

    char buf[32];
    int len = tok->end - tok->start;
    if (len >= (int)sizeof(buf)) {
        return -1;
    }

    memcpy(buf, json + tok->start, len);
    buf[len] = '\0';

    char* endptr;
    long val = strtol(buf, &endptr, 10);
    if (endptr == buf || *endptr != '\0') {
        return -1;
    }

    *value = (int)val;
    return 0;
}

int json_token_touint64(const char* json, jsmntok_t* tok, uint64_t* value) {
    if (tok->type != JSMN_PRIMITIVE) {
        return -1;
    }

    char buf[32];
    int len = tok->end - tok->start;
    if (len >= (int)sizeof(buf)) {
        return -1;
    }

    memcpy(buf, json + tok->start, len);
    buf[len] = '\0';

    char* endptr;
    unsigned long long val = strtoull(buf, &endptr, 10);
    if (endptr == buf || *endptr != '\0') {
        return -1;
    }

    *value = (uint64_t)val;
    return 0;
}

int json_find_key(const char* json, jsmntok_t* tokens, jsmntok_t* obj_tok, const char* key) {
    if (obj_tok->type != JSMN_OBJECT) {
        return -1;
    }

    int obj_idx = obj_tok - tokens;
    int i = obj_idx + 1;  /* Start after object token */

    for (int count = 0; count < obj_tok->size; count++) {
        /* Each property is key-value pair */
        jsmntok_t* key_tok = &tokens[i];
        jsmntok_t* val_tok = &tokens[i + 1];

        if (json_token_streq(json, key_tok, key)) {
            return i + 1;  /* Return index of value token */
        }

        /* Skip to next property (skip value and its children) */
        i++;  /* Skip key */
        int skip = 1;
        while (skip > 0) {
            if (tokens[i].size > 0) {
                skip += tokens[i].size;
            }
            i++;
            skip--;
        }
    }

    return -1;
}

void json_output_string(const char* str) {
    putchar('"');
    for (const char* p = str; *p; p++) {
        switch (*p) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\b': printf("\\b"); break;
            case '\f': printf("\\f"); break;
            case '\n': printf("\\n"); break;
            case '\r': printf("\\r"); break;
            case '\t': printf("\\t"); break;
            default:
                if ((unsigned char)*p < 32) {
                    printf("\\u%04x", (unsigned char)*p);
                } else {
                    putchar(*p);
                }
        }
    }
    putchar('"');
}

void get_timestamp(char* buf, size_t bufsize) {
    time_t now = time(NULL);
    struct tm* tm_info = gmtime(&now);
    strftime(buf, bufsize, "%Y-%m-%dT%H:%M:%SZ", tm_info);
}
