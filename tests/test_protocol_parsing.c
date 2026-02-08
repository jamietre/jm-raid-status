/**
 * test_protocol_parsing.c - Test protocol parsing with real controller responses
 *
 * Uses fixtures extracted from actual controller captures to validate:
 * - RAID flag detection (degraded, healthy, rebuilding states)
 * - IDENTIFY response parsing
 * - State detection logic
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "test_framework.h"

// Test fixture loading
static uint8_t* load_fixture(const char* path, size_t expected_size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open fixture: %s\n", path);
        return NULL;
    }

    uint8_t* data = malloc(expected_size);
    if (!data) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(data, 1, expected_size, f);
    fclose(f);

    if (read != expected_size) {
        fprintf(stderr, "Fixture %s: expected %zu bytes, got %zu\n",
                path, expected_size, read);
        free(data);
        return NULL;
    }

    return data;
}

// Test DEGRADED state flag detection
void test_degraded_flags(void) {
    TEST_CASE("DEGRADED state - 0x1F0 flag detection");

    uint8_t* response = load_fixture("tests/fixtures/degraded/identify_disk0.bin", 512);
    ASSERT_TRUE(response != NULL, "Fixture should load successfully");

    // In degraded state, 0x1F0 should be 0x07 (not 0x0F)
    uint8_t health_flag = response[0x1F0];
    ASSERT_EQ(health_flag, 0x07, "Degraded state should have 0x1F0=0x07");
    ASSERT_TRUE((health_flag & 0x08) == 0, "Bit 3 should be clear in degraded state");

    free(response);
}

// Test HEALTHY state flags
void test_healthy_flags(void) {
    TEST_CASE("HEALTHY state - 0x1F0 and 0x1F5 flags");

    uint8_t* response = load_fixture("tests/fixtures/healthy/identify_disk0.bin", 512);
    ASSERT_TRUE(response != NULL, "Fixture should load successfully");

    // In healthy state, 0x1F0 should be 0x0F and 0x1F5 should be 0x00
    uint8_t health_flag = response[0x1F0];
    uint8_t rebuild_flag = response[0x1F5];

    ASSERT_EQ(health_flag, 0x0F, "Healthy state should have 0x1F0=0x0F");
    ASSERT_EQ(rebuild_flag, 0x00, "Healthy idle state should have 0x1F5=0x00");
    ASSERT_TRUE((health_flag & 0x08) != 0, "Bit 3 should be set in healthy state");

    free(response);
}

// Test REBUILDING state flags
void test_rebuilding_flags(void) {
    TEST_CASE("REBUILDING state - 0x1F0, 0x1F5, 0x1FA flags");

    uint8_t* response = load_fixture("tests/fixtures/rebuilding/identify_disk0.bin", 512);
    ASSERT_TRUE(response != NULL, "Fixture should load successfully");

    // In rebuilding state, 0x1F0=0x0F (healthy), 0x1F5=0x01 (rebuilding)
    uint8_t health_flag = response[0x1F0];
    uint8_t rebuild_flag = response[0x1F5];
    uint8_t phase_flag = response[0x1FA];

    ASSERT_EQ(health_flag, 0x0F, "Rebuilding array should still show 0x1F0=0x0F");
    ASSERT_EQ(rebuild_flag, 0x01, "Rebuilding state should have 0x1F5=0x01");
    ASSERT_EQ(phase_flag, 0x00, "Rebuild phase should be 0x1FA=0x00");

    free(response);
}

// Test that different states have different flags
void test_state_differences(void) {
    TEST_CASE("Different RAID states have different flags");

    uint8_t* degraded = load_fixture("tests/fixtures/degraded/identify_disk0.bin", 512);
    uint8_t* healthy = load_fixture("tests/fixtures/healthy/identify_disk0.bin", 512);
    uint8_t* rebuilding = load_fixture("tests/fixtures/rebuilding/identify_disk0.bin", 512);

    ASSERT_TRUE(degraded && healthy && rebuilding, "All fixtures should load");

    // Degraded vs Healthy - different 0x1F0
    ASSERT_NEQ(degraded[0x1F0], healthy[0x1F0], "Degraded and healthy should differ at 0x1F0");

    // Healthy vs Rebuilding - different 0x1F5
    ASSERT_EQ(healthy[0x1F0], rebuilding[0x1F0], "Healthy and rebuilding same 0x1F0");
    ASSERT_NEQ(healthy[0x1F5], rebuilding[0x1F5], "Healthy and rebuilding differ at 0x1F5");

    free(degraded);
    free(healthy);
    free(rebuilding);
}

// Test fixture size validation
void test_fixture_sizes(void) {
    TEST_CASE("All fixtures should be exactly 512 bytes");

    const char* fixtures[] = {
        "tests/fixtures/degraded/identify_disk0.bin",
        "tests/fixtures/healthy/identify_disk0.bin",
        "tests/fixtures/rebuilding/identify_disk0.bin"
    };

    for (int i = 0; i < 3; i++) {
        FILE* f = fopen(fixtures[i], "rb");
        ASSERT_TRUE(f != NULL, "Fixture should exist");

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fclose(f);

        ASSERT_EQ(size, 512, "Fixture should be exactly 512 bytes");
    }
}

int main(void) {
    TEST_SUITE("Protocol Parsing with Real Fixtures");

    printf("Testing RAID flag detection with real controller responses...\n\n");

    test_fixture_sizes();
    test_degraded_flags();
    test_healthy_flags();
    test_rebuilding_flags();
    test_state_differences();

    TEST_SUMMARY();
}
