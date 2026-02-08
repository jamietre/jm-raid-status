/*
 * test_framework.h - Lightweight testing framework for jm-raid-status
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Test statistics */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* ANSI color codes for output */
#define COLOR_GREEN  "\033[32m"
#define COLOR_RED    "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET  "\033[0m"

/* Test assertion macros */
#define ASSERT_TRUE(expr, msg) do { \
    tests_run++; \
    if (expr) { \
        tests_passed++; \
        printf("  " COLOR_GREEN "✓" COLOR_RESET " %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  " COLOR_RED "✗" COLOR_RESET " %s\n", msg); \
        printf("    Failed: %s (line %d)\n", #expr, __LINE__); \
    } \
} while(0)

#define ASSERT_FALSE(expr, msg) ASSERT_TRUE(!(expr), msg)

#define ASSERT_EQ(a, b, msg) do { \
    tests_run++; \
    if ((a) == (b)) { \
        tests_passed++; \
        printf("  " COLOR_GREEN "✓" COLOR_RESET " %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  " COLOR_RED "✗" COLOR_RESET " %s\n", msg); \
        printf("    Expected: %d, Got: %d (line %d)\n", (int)(b), (int)(a), __LINE__); \
    } \
} while(0)

#define ASSERT_NEQ(a, b, msg) do { \
    tests_run++; \
    if ((a) != (b)) { \
        tests_passed++; \
        printf("  " COLOR_GREEN "✓" COLOR_RESET " %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  " COLOR_RED "✗" COLOR_RESET " %s\n", msg); \
        printf("    Values should not be equal: %d (line %d)\n", (int)(a), __LINE__); \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b, msg) do { \
    tests_run++; \
    if (strcmp((a), (b)) == 0) { \
        tests_passed++; \
        printf("  " COLOR_GREEN "✓" COLOR_RESET " %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  " COLOR_RED "✗" COLOR_RESET " %s\n", msg); \
        printf("    Expected: \"%s\", Got: \"%s\" (line %d)\n", (b), (a), __LINE__); \
    } \
} while(0)

#define ASSERT_MEM_EQ(a, b, len, msg) do { \
    tests_run++; \
    if (memcmp((a), (b), (len)) == 0) { \
        tests_passed++; \
        printf("  " COLOR_GREEN "✓" COLOR_RESET " %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  " COLOR_RED "✗" COLOR_RESET " %s\n", msg); \
        printf("    Memory regions differ (line %d)\n", __LINE__); \
    } \
} while(0)

/* Test suite macros */
#define TEST_SUITE(name) \
    printf("\n" COLOR_YELLOW "=== " name " ===" COLOR_RESET "\n")

#define TEST_CASE(name) \
    printf("\n" name ":\n")

#define TEST_SUMMARY() do { \
    printf("\n" COLOR_YELLOW "==== Test Summary ====" COLOR_RESET "\n"); \
    printf("Total:  %d tests\n", tests_run); \
    printf("Passed: " COLOR_GREEN "%d" COLOR_RESET "\n", tests_passed); \
    if (tests_failed > 0) { \
        printf("Failed: " COLOR_RED "%d" COLOR_RESET "\n", tests_failed); \
    } else { \
        printf("Failed: 0\n"); \
    } \
    printf("\n"); \
    if (tests_failed > 0) { \
        printf(COLOR_RED "TESTS FAILED" COLOR_RESET "\n"); \
        return 1; \
    } else { \
        printf(COLOR_GREEN "ALL TESTS PASSED" COLOR_RESET "\n"); \
        return 0; \
    } \
} while(0)

#endif /* TEST_FRAMEWORK_H */
