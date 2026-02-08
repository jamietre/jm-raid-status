/*
 * test_output_formatter.c - Part of jm-raid-status
 * Tests for output formatting functions
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "../src/output_formatter.h"
#include "../src/smart_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Helper: Redirect stdout to a buffer for testing */
static char output_buffer[32768];
static FILE* original_stdout;
static FILE* mem_stream;

static void setup_output_capture(void) {
    fflush(stdout);
    original_stdout = stdout;
    mem_stream = fmemopen(output_buffer, sizeof(output_buffer), "w");
    if (mem_stream) {
        stdout = mem_stream;
    }
}

static void teardown_output_capture(void) {
    if (mem_stream) {
        fflush(mem_stream);
        fclose(mem_stream);
        stdout = original_stdout;
    }
}

static const char* get_captured_output(void) {
    return output_buffer;
}

/* Helper: Simple JSON validation - checks for basic structure */
static int is_valid_json(const char* json) {
    int brace_count = 0;
    int bracket_count = 0;
    int in_string = 0;
    int escape_next = 0;

    for (const char* p = json; *p; p++) {
        if (escape_next) {
            escape_next = 0;
            continue;
        }

        if (*p == '\\') {
            escape_next = 1;
            continue;
        }

        if (*p == '"') {
            in_string = !in_string;
            continue;
        }

        if (in_string) {
            continue;
        }

        if (*p == '{') brace_count++;
        if (*p == '}') brace_count--;
        if (*p == '[') bracket_count++;
        if (*p == ']') bracket_count--;

        /* Negative counts mean mismatched brackets */
        if (brace_count < 0 || bracket_count < 0) {
            return 0;
        }
    }

    /* All brackets should be balanced */
    return (brace_count == 0 && bracket_count == 0 && !in_string);
}

/* Helper: Check if JSON contains a key */
static int json_contains_key(const char* json, const char* key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    return strstr(json, search) != NULL;
}

/* Test: JSON output has valid structure */
void test_json_valid_structure(void) {
    TEST_CASE("JSON output has valid structure");

    /* Create minimal disk data */
    disk_smart_data_t disks[5] = {0};
    disks[0].is_present = 1;
    disks[0].disk_number = 0;
    strncpy(disks[0].disk_name, "TEST_DISK", sizeof(disks[0].disk_name) - 1);
    disks[0].overall_status = DISK_STATUS_PASSED;

    setup_output_capture();
    format_json("/dev/sdX", disks, 1, 0, 0, 0);
    teardown_output_capture();

    const char* output = get_captured_output();

    ASSERT_TRUE(is_valid_json(output), "JSON should have balanced braces and brackets");
    ASSERT_TRUE(json_contains_key(output, "version"), "JSON should contain version key");
    ASSERT_TRUE(json_contains_key(output, "device"), "JSON should contain device key");
    ASSERT_TRUE(json_contains_key(output, "disks"), "JSON should contain disks array");
}

/* Test: JSON includes RAID status information */
void test_json_includes_raid_status(void) {
    TEST_CASE("JSON output includes RAID status information");

    disk_smart_data_t disks[5] = {0};
    disks[0].is_present = 1;
    disks[0].disk_number = 0;
    disks[0].overall_status = DISK_STATUS_PASSED;

    setup_output_capture();
    format_json("/dev/sdX", disks, 1, 4, 3, 1);  /* degraded: expect 4, found 3 */
    teardown_output_capture();

    const char* output = get_captured_output();

    ASSERT_TRUE(is_valid_json(output), "JSON should be valid even with RAID status");
    ASSERT_TRUE(json_contains_key(output, "raid_status"), "JSON should contain raid_status object");
    ASSERT_TRUE(json_contains_key(output, "status"), "raid_status should contain status field");
    ASSERT_TRUE(json_contains_key(output, "expected_disks"), "raid_status should contain expected_disks");
    ASSERT_TRUE(json_contains_key(output, "present_disks"), "raid_status should contain present_disks");
    ASSERT_TRUE(json_contains_key(output, "issues"), "raid_status should contain issues array");

    /* Check for degraded status */
    ASSERT_TRUE(strstr(output, "\"status\": \"degraded\"") != NULL,
                "Should report degraded status when fewer disks than expected");
}

/* Test: JSON degraded status with issues */
void test_json_degraded_with_issues(void) {
    TEST_CASE("JSON shows degraded RAID with issue description");

    disk_smart_data_t disks[5] = {0};
    disks[0].is_present = 1;
    disks[0].overall_status = DISK_STATUS_PASSED;

    setup_output_capture();
    format_json("/dev/sdX", disks, 1, 5, 4, 1);  /* degraded: expect 5, found 4 */
    teardown_output_capture();

    const char* output = get_captured_output();

    ASSERT_TRUE(is_valid_json(output), "JSON should be valid");
    ASSERT_TRUE(strstr(output, "Degraded") != NULL, "Issues should describe degraded state");
    ASSERT_TRUE(strstr(output, "Expected 5") != NULL, "Issues should mention expected count");
}

/* Test: JSON oversized array status */
void test_json_oversized_array(void) {
    TEST_CASE("JSON shows oversized array status");

    disk_smart_data_t disks[5] = {0};
    for (int i = 0; i < 5; i++) {
        disks[i].is_present = 1;
        disks[i].overall_status = DISK_STATUS_PASSED;
    }

    setup_output_capture();
    format_json("/dev/sdX", disks, 5, 4, 5, 0);  /* oversized: expect 4, found 5 */
    teardown_output_capture();

    const char* output = get_captured_output();

    ASSERT_TRUE(is_valid_json(output), "JSON should be valid");
    ASSERT_TRUE(strstr(output, "\"status\": \"oversized\"") != NULL,
                "Should report oversized status");
    ASSERT_TRUE(strstr(output, "Oversized") != NULL, "Issues should describe oversized state");
}

/* Test: JSON failed disk status */
void test_json_failed_disk(void) {
    TEST_CASE("JSON shows failed disk in issues");

    disk_smart_data_t disks[5] = {0};
    disks[0].is_present = 1;
    disks[0].disk_number = 0;
    strncpy(disks[0].disk_name, "WDC_DISK", sizeof(disks[0].disk_name) - 1);
    disks[0].overall_status = DISK_STATUS_FAILED;

    setup_output_capture();
    format_json("/dev/sdX", disks, 1, 0, 0, 0);  /* single failed disk */
    teardown_output_capture();

    const char* output = get_captured_output();

    ASSERT_TRUE(is_valid_json(output), "JSON should be valid");
    ASSERT_TRUE(strstr(output, "\"status\": \"failed\"") != NULL,
                "Should report failed status");
    ASSERT_TRUE(strstr(output, "Disk 0") != NULL && strstr(output, "SMART health check failed") != NULL,
                "Issues should describe failed disk");
}

/* Test: JSON healthy status with no issues */
void test_json_healthy_no_issues(void) {
    TEST_CASE("JSON shows healthy status with empty issues array");

    disk_smart_data_t disks[5] = {0};
    for (int i = 0; i < 4; i++) {
        disks[i].is_present = 1;
        disks[i].overall_status = DISK_STATUS_PASSED;
    }

    setup_output_capture();
    format_json("/dev/sdX", disks, 4, 4, 4, 0);  /* healthy: all good */
    teardown_output_capture();

    const char* output = get_captured_output();

    ASSERT_TRUE(is_valid_json(output), "JSON should be valid");
    ASSERT_TRUE(strstr(output, "\"status\": \"healthy\"") != NULL,
                "Should report healthy status");
    ASSERT_TRUE(strstr(output, "\"issues\": []") != NULL,
                "Should have empty issues array when healthy");
}

/* Test: No extraneous output after JSON */
void test_json_no_extraneous_output(void) {
    TEST_CASE("JSON output contains only JSON (no trailing text)");

    disk_smart_data_t disks[5] = {0};
    disks[0].is_present = 1;
    disks[0].overall_status = DISK_STATUS_PASSED;

    setup_output_capture();
    format_json("/dev/sdX", disks, 1, 5, 4, 1);  /* degraded */
    teardown_output_capture();

    const char* output = get_captured_output();

    /* Find the last closing brace */
    const char* last_brace = strrchr(output, '}');
    ASSERT_TRUE(last_brace != NULL, "Should have closing brace");

    /* After the last brace, there should only be whitespace */
    const char* p = last_brace + 1;
    while (*p) {
        ASSERT_TRUE(*p == ' ' || *p == '\n' || *p == '\t' || *p == '\r',
                    "No non-whitespace content should appear after closing JSON brace");
        p++;
    }
}

int main(void) {
    TEST_SUITE("Output Formatter Tests");

    test_json_valid_structure();
    test_json_includes_raid_status();
    test_json_degraded_with_issues();
    test_json_oversized_array();
    test_json_failed_disk();
    test_json_healthy_no_issues();
    test_json_no_extraneous_output();

    TEST_SUMMARY();
}
