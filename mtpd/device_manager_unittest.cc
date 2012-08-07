// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mtpd/device_manager.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace mtpd {
namespace {

TEST(DeviceManagerTest, ParseStorageName) {
  struct ParseStorageNameTestCase {
    const char* input;
    bool expected_result;
    const char* expected_bus;
    uint32_t expected_storage_id;
  } test_cases[] = {
    { "usb:123:4", true, "usb:123", 4 },
    { "usb:1,2,3:4", true, "usb:1,2,3", 4 },
    { "notusb:123:4", false, "", 0 },
    { "usb:123:4:badfield", false, "", 0 },
    { "usb:123:not_number", false, "", 0 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    std::string bus;
    uint32_t storage_id = static_cast<uint32_t>(-1);
    bool result =
        DeviceManager::ParseStorageName(test_cases[i].input, &bus, &storage_id);
    EXPECT_EQ(test_cases[i].expected_result, result);
    if (test_cases[i].expected_result) {
      EXPECT_EQ(test_cases[i].expected_bus, bus);
      EXPECT_EQ(test_cases[i].expected_storage_id, storage_id);
    }
  }
}

}  // namespace
}  // namespace mtp
