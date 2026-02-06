/*
 * jm_crc.c - CRC-32 calculation for JMicron protocol
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#include "jm_crc.h"
#include <stdint.h>
#include <stddef.h>

/* CRC-32 polynomial (IEEE 802.3): x^32 + x^26 + x^23 + ... + x^2 + x + 1 */
#define CRC32_POLY 0x04C11DB7

/* JMicron protocol uses this specific initial value */
#define JM_CRC_SEED 0x52325032

/* Pre-computed CRC table for faster calculation */
static uint32_t crc_table[256];
static int table_initialized = 0;

/*
 * Initialize CRC lookup table
 * Uses standard CRC-32 algorithm with polynomial 0x04C11DB7
 */
static void init_crc_table(void)
{
    for (unsigned int i = 0; i < 256; i++)
    {
        uint32_t crc = i << 24;
        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x80000000)
            {
                crc = (crc << 1) ^ CRC32_POLY;
            }
            else
            {
                crc = crc << 1;
            }
        }
        crc_table[i] = crc;
    }
    table_initialized = 1;
}

/*
 * Calculate CRC-32 for JMicron protocol
 *
 * Processes data as 32-bit words in big-endian (network) byte order.
 * Uses JMicron-specific seed value and no final XOR.
 *
 * @param data Pointer to data buffer (32-bit words)
 * @param num_words Number of 32-bit words to process
 * @return Calculated CRC-32 value
 */
uint32_t JM_CRC(uint32_t *data, uint32_t num_words)
{
    if (!table_initialized)
    {
        init_crc_table();
    }

    uint32_t crc = JM_CRC_SEED;

    for (uint32_t i = 0; i < num_words; i++)
    {
        /* Convert to big-endian (network order) */
        uint32_t word = __builtin_bswap32(data[i]);

        /* Process each byte through CRC table */
        crc = crc_table[(word & 0xFF) ^ (crc >> 24)] ^ (crc << 8);
        crc = crc_table[((word >> 8) & 0xFF) ^ (crc >> 24)] ^ (crc << 8);
        crc = crc_table[((word >> 16) & 0xFF) ^ (crc >> 24)] ^ (crc << 8);
        crc = crc_table[(word >> 24) ^ (crc >> 24)] ^ (crc << 8);
    }

    return crc;
}
