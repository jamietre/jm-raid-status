/*
 * test_xor.c - Unit tests for SATA XOR scrambling
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "../src/sata_xor.h"
#include <stdint.h>
#include <string.h>

void test_xor_reversible(void) {
    TEST_CASE("XOR scrambling is reversible");

    uint32_t original[128];
    uint32_t scrambled[128];

    /* Create test data */
    for (int i = 0; i < 128; i++) {
        original[i] = 0x12345678 + i;
    }

    /* Copy for scrambling */
    memcpy(scrambled, original, sizeof(original));

    /* Scramble */
    SATA_XOR(scrambled);

    /* Verify it changed */
    int changed = 0;
    for (int i = 0; i < 128; i++) {
        if (scrambled[i] != original[i]) {
            changed = 1;
            break;
        }
    }
    ASSERT_TRUE(changed, "XOR should modify the data");

    /* Unscramble (XOR again) */
    SATA_XOR(scrambled);

    /* Verify it's back to original */
    ASSERT_MEM_EQ(scrambled, original, sizeof(original), "Double XOR should restore original");
}

void test_xor_zeros(void) {
    TEST_CASE("XOR on zero buffer");

    uint32_t buffer[128] = {0};
    uint32_t expected[128] = {0};

    SATA_XOR(buffer);

    /* XOR of zeros should produce non-zero pattern */
    int all_zeros = 1;
    for (int i = 0; i < 128; i++) {
        if (buffer[i] != 0) {
            all_zeros = 0;
            break;
        }
    }
    ASSERT_FALSE(all_zeros, "XOR of zeros should not be all zeros");

    /* XOR again should return to zeros */
    SATA_XOR(buffer);
    ASSERT_MEM_EQ(buffer, expected, sizeof(buffer), "Double XOR of zeros returns to zeros");
}

void test_xor_pattern(void) {
    TEST_CASE("XOR with specific pattern");

    uint32_t buffer[128];
    uint32_t backup[128];

    /* Fill with pattern */
    for (int i = 0; i < 128; i++) {
        buffer[i] = 0xAAAAAAAA;
        backup[i] = 0xAAAAAAAA;
    }

    /* Scramble and unscramble */
    SATA_XOR(buffer);
    SATA_XOR(buffer);

    ASSERT_MEM_EQ(buffer, backup, sizeof(buffer), "Pattern survives scramble/unscramble");
}

int main(void) {
    TEST_SUITE("SATA XOR Scrambling Tests");

    test_xor_reversible();
    test_xor_zeros();
    test_xor_pattern();

    TEST_SUMMARY();
}
