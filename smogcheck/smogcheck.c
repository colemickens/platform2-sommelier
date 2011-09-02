// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// smogcheck.c: demonstrate use of lib_smogcheck.c

#include <stdlib.h>
#include "lib/lib_smogcheck.h"

void ExitOnError(int val) {
  if (val < 0) {
    exit(EXIT_FAILURE);
  }
}

int main() {
  int fd = -1;
  int ret = -1;
  const int kPcaSlaveAddress = 0x27;
  const int kI2cBusAddress = 0x2;
  const int kInaSlaveAddress = 0x40;
  const __u8 kLedRegister = 0x3;
  const __u8 kVoltageRegister = 0x2;
  const __u8 kTurnOnLedOne = 0xfe;

  fd = GetDeviceFile(kI2cBusAddress);
  ExitOnError(fd);

  // Enable LED 1
  ret = SetSlaveAddress(fd, kPcaSlaveAddress);
  ExitOnError(ret);

  ret = WriteByte(fd, kLedRegister, kTurnOnLedOne);
  ExitOnError(ret);

  ret = ReadByte(fd, kLedRegister);
  ExitOnError(ret);

  // Read Backup Power voltage
  ret = SetSlaveAddress(fd, kInaSlaveAddress);
  ExitOnError(ret);

  ret = ReadWord(fd, kVoltageRegister);
  ExitOnError(ret);

  return EXIT_SUCCESS;
}
