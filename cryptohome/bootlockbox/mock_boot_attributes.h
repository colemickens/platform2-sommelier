// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_BOOTLOCKBOX_MOCK_BOOT_ATTRIBUTES_H_
#define CRYPTOHOME_BOOTLOCKBOX_MOCK_BOOT_ATTRIBUTES_H_

#include <string>

#include <gmock/gmock.h>

#include "cryptohome/bootlockbox/boot_attributes.h"

namespace cryptohome {

class MockBootAttributes : public BootAttributes {
 public:
  MockBootAttributes() : BootAttributes(NULL, NULL) {}
  virtual ~MockBootAttributes() {}

  MOCK_METHOD(bool, Load, (), (override));
  MOCK_METHOD(bool, Get, (const std::string&, std::string*), (const, override));
  MOCK_METHOD(void, Set, (const std::string&, const std::string&), (override));
  MOCK_METHOD(bool, FlushAndSign, (), (override));
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_BOOTLOCKBOX_MOCK_BOOT_ATTRIBUTES_H_
