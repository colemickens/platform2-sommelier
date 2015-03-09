// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/container_spec_wrapper.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <gtest/gtest.h>

namespace soma {

class ContainerSpecWrapperTest : public ::testing::Test {
 public:
  ContainerSpecWrapperTest() {}
  virtual ~ContainerSpecWrapperTest() {}
};

TEST_F(ContainerSpecWrapperTest, DevicePathFilterTest) {
  ContainerSpecWrapper spec(base::FilePath("/foo/bar"), 0, 0);
  std::string device_path("/dev/thing");
  parser::DevicePathFilterSet filters;
  filters.insert(parser::DevicePathFilter(base::FilePath(device_path)));
  spec.SetDevicePathFilters(filters);

  EXPECT_TRUE(spec.DevicePathIsAllowed(base::FilePath(device_path)));
  EXPECT_FALSE(spec.DevicePathIsAllowed(base::FilePath("/not/a/thing")));
}

TEST_F(ContainerSpecWrapperTest, DeviceNodeFilterTest) {
  ContainerSpecWrapper spec(base::FilePath("/foo/bar"), 0, 0);
  parser::DeviceNodeFilterSet filters;
  filters.insert(parser::DeviceNodeFilter(1, 2));
  spec.SetDeviceNodeFilters(filters);

  EXPECT_TRUE(spec.DeviceNodeIsAllowed(1, 2));
  EXPECT_FALSE(spec.DeviceNodeIsAllowed(0, 1));
}

}  // namespace soma
