// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/at_exit.h>
#include <chromeos/dbus/dbus.h>
#include <gtest/gtest.h>

#include "make_tests.h"

const char* kDefaultImageDir = "test_image_dir";
const char* kAltImageDir = "alt_test_image_dir";

int main(int argc, char **argv) {
  base::AtExitManager exit_manager;
  cryptohome::MakeTests make_tests;
  make_tests.InitTestData(std::string(kDefaultImageDir),
                          cryptohome::kDefaultUsers,
                          cryptohome::kDefaultUserCount);

  ::g_type_init();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
