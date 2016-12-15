// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "userspace_touchpad/i2c-device.h"

using std::string;

I2cDevice::I2cDevice(string i2cBus, uint8_t slaveAddress) {
  i2c_fd_ = open(i2cBus.c_str(), O_RDWR);
  if (i2c_fd_ == -1) {
    fprintf(stderr, "Failed to open device `%s'\n", i2cBus.c_str());
    exit(1);
  }
  if (ioctl(i2c_fd_, I2C_SLAVE_FORCE, slaveAddress)) {
    fprintf(stderr, "Failed to set slave address 0x%x\n", slaveAddress);
    exit(1);
  }
}

bool I2cDevice::Write(uint8_t* data, const int length) {
  // Note that the file descriptor handles all the low-level I2C transactions
  // for us so we can read/write it like a normal file.
  return write(i2c_fd_, data, length) == length;
}

bool I2cDevice::Read(uint8_t* data, const int length) {
  return read(i2c_fd_, data, length) == length;
}
