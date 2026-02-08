/*
 * test_crc.c - Unit tests for JMicron CRC calculation
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "test_framework.h"
#include "../src/jm_crc.h"
#include <stdint.h>
#include <string.h>

void test_crc_empty_buffer(void) {
    TEST_CASE("CRC of empty buffer");

    uint32_t buffer[128] = {0};
    uint32_t crc1 = JM_CRC(buffer, 0);
    uint32_t crc2 = JM_CRC(buffer, 0);

    ASSERT_EQ(crc1, crc2, "CRC of zero-length buffer should be consistent");
}

void test_crc_known_values(void) {
    TEST_CASE("CRC with known values");

    uint32_t buffer[128];
    memset(buffer, 0, sizeof(buffer));

    /* Test vector 1: All zeros */
    buffer[0] = 0x00000000;
    buffer[1] = 0x00000000;
    uint32_t crc1 = JM_CRC(buffer, 2);
    ASSERT_NEQ(crc1, 0, "CRC of non-empty buffer should not be zero");

    /* Test vector 2: Known pattern */
    buffer[0] = 0x197b0325;  /* JMicron magic number */
    buffer[1] = 0x00000001;
    uint32_t crc2 = JM_CRC(buffer, 2);
    ASSERT_NEQ(crc2, crc1, "Different data should produce different CRC");
}

void test_crc_consistency(void) {
    TEST_CASE("CRC consistency (same input = same output)");

    uint32_t buffer[128];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 0x12345678;
    buffer[1] = 0x9abcdef0;

    uint32_t crc1 = JM_CRC(buffer, 2);
    uint32_t crc2 = JM_CRC(buffer, 2);

    ASSERT_EQ(crc1, crc2, "CRC should be deterministic");
}

void test_crc_different_lengths(void) {
    TEST_CASE("CRC with different lengths");

    uint32_t buffer[128];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = 0x11111111;
    buffer[1] = 0x22222222;
    buffer[2] = 0x33333333;

    uint32_t crc_short = JM_CRC(buffer, 2);
    uint32_t crc_long = JM_CRC(buffer, 3);

    ASSERT_NEQ(crc_short, crc_long, "Different lengths should produce different CRCs");
}

int main(void) {
    TEST_SUITE("JMicron CRC Tests");

    test_crc_empty_buffer();
    test_crc_known_values();
    test_crc_consistency();
    test_crc_different_lengths();

    TEST_SUMMARY();
}
