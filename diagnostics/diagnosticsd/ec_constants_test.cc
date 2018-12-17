// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>

#include <gtest/gtest.h>

#include "diagnostics/diagnosticsd/ec_constants.h"

namespace diagnostics {

TEST(EcConstantsTest, PropertiesPath) {
  EXPECT_EQ(
      base::FilePath(kEcDriverSysfsPath).Append(kEcDriverSysfsPropertiesPath),
      base::FilePath("sys/bus/platform/devices/GOOG000C:00/properties/"));
}

TEST(EcConstantsTest, RawFilePath) {
  EXPECT_EQ(base::FilePath(kEcDriverSysfsPath).Append(kEcRunCommandFilePath),
            base::FilePath("sys/bus/platform/devices/GOOG000C:00/raw"));
}

}  // namespace diagnostics
