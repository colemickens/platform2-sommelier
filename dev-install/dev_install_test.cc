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
  MOCK_CONST_METHOD0(IsDevMode, bool());
};

class DevInstallTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Set the default to dev mode enabled.  Most tests want that.
    ON_CALL(dev_install_, IsDevMode()).WillByDefault(Return(true));
  }

 protected:
  DevInstallMock dev_install_;
};

}  // namespace

// Check default run through.
TEST_F(DevInstallTest, Run) {
  EXPECT_CALL(dev_install_, Exec(_)).WillOnce(Return(1234));
  EXPECT_EQ(1234, dev_install_.Run());
}

// Systems not in dev mode should abort.
TEST_F(DevInstallTest, NonDevMode) {
  EXPECT_CALL(dev_install_, IsDevMode()).WillOnce(Return(false));
  EXPECT_CALL(dev_install_, Exec(_)).Times(0);
  EXPECT_EQ(2, dev_install_.Run());
}

}  // namespace dev_install
