// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dev-install/dev_install.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::Return;

namespace dev_install {

namespace {

class DevInstallMock : public DevInstall {
 public:
  MOCK_METHOD1(Exec, int(const std::vector<const char*>&));
};

class DevInstallTest : public ::testing::Test {
 protected:
  DevInstallMock dev_install_;
};

}  // namespace

// Check default run through.
TEST_F(DevInstallTest, Run) {
  EXPECT_CALL(dev_install_, Exec(_)).WillOnce(Return(1234));
  EXPECT_EQ(1234, dev_install_.Run());
}

}  // namespace dev_install
