// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/container_spec.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <gtest/gtest.h>

namespace soma {

class ContainerSpecTest : public ::testing::Test {
 public:
  ContainerSpecTest() {}
  virtual ~ContainerSpecTest() {}
};

TEST_F(ContainerSpecTest, DevicePathFilterTest) {
  base::ScopedTempDir tmpdir;
  base::FilePath scratch;
  ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(tmpdir.path(), &scratch));

  ContainerSpec spec(base::FilePath("/foo/bar"), 0, 0);
  std::string device_path("/dev/thing");
  spec.AddDevicePathFilter(device_path);

  EXPECT_TRUE(spec.DevicePathIsAllowed(base::FilePath(device_path)));
  EXPECT_FALSE(spec.DevicePathIsAllowed(base::FilePath("/not/a/thing")));
}

TEST_F(ContainerSpecTest, DeviceNodeFilterTest) {
  base::ScopedTempDir tmpdir;
  base::FilePath scratch;
  ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(tmpdir.path(), &scratch));

  ContainerSpec spec(base::FilePath("/foo/bar"), 0, 0);
  spec.AddDeviceNodeFilter(1, 2);

  EXPECT_TRUE(spec.DeviceNodeIsAllowed(1, 2));
  EXPECT_FALSE(spec.DeviceNodeIsAllowed(0, 1));
}

}  // namespace soma
