// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMOGCHECK_INCLUDE_LIB_LIB_SMOGCHECK_H_
#define SMOGCHECK_INCLUDE_LIB_LIB_SMOGCHECK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMOGCHECK_EXPORT __attribute__((__visibility__("default")))

SMOGCHECK_EXPORT int GetDeviceFile(int adapter_nr);
SMOGCHECK_EXPORT int SetSlaveAddress(int fd, int addr);
SMOGCHECK_EXPORT int WriteByte(int fd, uint8_t reg, uint8_t byte_val);
SMOGCHECK_EXPORT int ReadByte(int fd, uint8_t reg);
SMOGCHECK_EXPORT int WriteWord(int fd, uint8_t reg, uint16_t word_val);
SMOGCHECK_EXPORT int ReadWord(int fd, uint8_t reg);

#ifdef __cplusplus
}
#endif

#endif  // SMOGCHECK_INCLUDE_LIB_LIB_SMOGCHECK_H_
