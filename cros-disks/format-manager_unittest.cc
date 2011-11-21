// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/format-manager.h"

#include <gtest/gtest.h>

using std::map;
using std::string;
using std::vector;

namespace cros_disks {

class FormatManagerTest : public ::testing::Test {
 protected:
  FormatManager manager_;
};

TEST_F(FormatManagerTest, GetFormatProgramPath) {
  EXPECT_EQ("", manager_.GetFormatProgramPath("nonexistent-fs"));
}

TEST_F(FormatManagerTest, IsFilesystemSupported) {
  EXPECT_TRUE(manager_.IsFilesystemSupported("vfat"));
  EXPECT_FALSE(manager_.IsFilesystemSupported("nonexistent-fs"));
}

}  // namespace cros_disks
