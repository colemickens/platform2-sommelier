// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/usb_utils.h"

#include <base/files/file_path.h>
#include <gtest/gtest.h>

using base::FilePath;

namespace oobe_config {

class UtilsTest : public ::testing::Test {};

TEST_F(UtilsTest, ScopedPathUnlinker) {
  FilePath temp_file;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_file));
  {
    ScopedPathUnlinker unlinker(temp_file);
    EXPECT_TRUE(base::PathExists(temp_file));
  }
  EXPECT_FALSE(base::PathExists(temp_file));
}

}  // namespace oobe_config
