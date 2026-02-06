/*
 * jm_crc.h - CRC-32 calculation for JMicron protocol
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef JM_CRC_H
#define JM_CRC_H

#include <stdint.h>

/*
 * Calculate CRC-32 for JMicron protocol
 *
 * @param theData Pointer to data buffer (32-bit words)
 * @param numDwords Number of 32-bit words to process
 * @return Calculated CRC-32 value
 */
uint32_t JM_CRC(uint32_t* theData, uint32_t numDwords);

#endif /* JM_CRC_H */
