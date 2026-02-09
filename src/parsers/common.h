/*
 * common.h - Common parser utilities
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef PARSERS_COMMON_H
#define PARSERS_COMMON_H

#include <stdint.h>
#include "../jsmn/jsmn.h"

/* Maximum JSON input size (10MB) */
#define MAX_JSON_INPUT_SIZE (10 * 1024 * 1024)

/* Maximum tokens for JSON parsing */
#define MAX_JSON_TOKENS 10000

/**
 * Read all input from stdin
 * @param size Output size of data read
 * @return Allocated buffer with stdin contents (caller must free), or NULL on error
 */
char* read_all_stdin(size_t* size);

/**
 * Helper: Check if JSON token matches a string
 * @param json JSON string
 * @param tok Token to check
 * @param s String to compare against
 * @return 1 if matches, 0 otherwise
 */
int json_token_streq(const char* json, jsmntok_t* tok, const char* s);

/**
 * Helper: Extract string value from JSON token
 * @param json JSON string
 * @param tok Token to extract
 * @param buf Output buffer
 * @param bufsize Size of output buffer
 * @return 0 on success, -1 on error
 */
int json_token_tostr(const char* json, jsmntok_t* tok, char* buf, size_t bufsize);

/**
 * Helper: Extract integer value from JSON token
 * @param json JSON string
 * @param tok Token to extract
 * @param value Output integer value
 * @return 0 on success, -1 on error
 */
int json_token_toint(const char* json, jsmntok_t* tok, int* value);

/**
 * Helper: Extract uint64 value from JSON token
 * @param json JSON string
 * @param tok Token to extract
 * @param value Output uint64 value
 * @return 0 on success, -1 on error
 */
int json_token_touint64(const char* json, jsmntok_t* tok, uint64_t* value);

/**
 * Helper: Find object key in JSON object
 * @param json JSON string
 * @param tokens Token array
 * @param obj_tok Object token to search in
 * @param key Key name to find
 * @return Token index of value, or -1 if not found
 */
int json_find_key(const char* json, jsmntok_t* tokens, jsmntok_t* obj_tok, const char* key);

/**
 * Output compact JSON string (escape special chars)
 * @param str Input string
 */
void json_output_string(const char* str);

/**
 * Get current timestamp in ISO 8601 format
 * @param buf Output buffer
 * @param bufsize Size of output buffer
 */
void get_timestamp(char* buf, size_t bufsize);

#endif /* PARSERS_COMMON_H */
