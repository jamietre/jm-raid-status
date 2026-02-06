/*
 * sata_xor.h - XOR scrambling for JMicron protocol
 *
 * Copyright (C) 2026 Jamie Treworgy
 * SPDX-License-Identifier: MIT
 */

#ifndef SATA_XOR_H
#define SATA_XOR_H

#include <stdint.h>

/*
 * Apply XOR scrambling/descrambling to 512-byte buffer
 *
 * @param theData Pointer to 512-byte buffer (128 32-bit words)
 */
void SATA_XOR(uint32_t* theData);

#endif /* SATA_XOR_H */
