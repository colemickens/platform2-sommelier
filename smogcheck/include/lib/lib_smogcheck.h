// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMOGCHECK_INCLUDE_LIB_LIB_SMOGCHECK_H_
#define SMOGCHECK_INCLUDE_LIB_LIB_SMOGCHECK_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int GetDeviceFile(int adapter_nr);
int SetSlaveAddress(int fd, int addr);
int WriteByte(int fd, uint8_t reg, uint8_t byte_val);
int ReadByte(int fd, uint8_t reg);
int WriteWord(int fd, uint8_t reg, uint16_t word_val);
int ReadWord(int fd, uint8_t reg);

#ifdef __cplusplus
}
#endif

#endif  // SMOGCHECK_INCLUDE_LIB_LIB_SMOGCHECK_H_
