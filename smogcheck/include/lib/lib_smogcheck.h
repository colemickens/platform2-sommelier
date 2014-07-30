// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMOGCHECK_INCLUDE_LIB_LIB_SMOGCHECK_H_
#define SMOGCHECK_INCLUDE_LIB_LIB_SMOGCHECK_H_

#include <linux/types.h>

int GetDeviceFile(int adapter_nr);
int SetSlaveAddress(int fd, int addr);
int WriteByte(int fd, __u8 reg, __u8 byte_val);
int ReadByte(int fd, __u8 reg);
int WriteWord(int fd, __u8 reg, __u16 word_val);
int ReadWord(int fd, __u8 reg);

#endif  // SMOGCHECK_INCLUDE_LIB_LIB_SMOGCHECK_H_
