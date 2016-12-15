// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USERSPACE_TOUCHPAD_I2C_DEVICE_H_
#define USERSPACE_TOUCHPAD_I2C_DEVICE_H_

#include <stdint.h>
#include <string>

// Class for I2C communication with the component.
class I2cDevice {
 public:
  // Example arguments:
  // i2cBus: "/dev/i2c-7"
  // slaveAddress: 0x46
  I2cDevice(std::string i2cBus, uint8_t slaveAddress);

  // I2C read/write.
  bool Write(uint8_t* data, const int length);
  bool Read(uint8_t* data, const int length);

 private:
  // I2C device file descriptor.
  int i2c_fd_;
};

#endif  // USERSPACE_TOUCHPAD_I2C_DEVICE_H_
