// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dev-install/dev_install.h"

#include <unistd.h>

#include <istream>
#include <sstream>
#include <string>

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
  MOCK_METHOD2(PromptUser, bool(std::istream&, const std::string&));
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

namespace {

class PromptUserTest : public ::testing::Test {
 protected:
  DevInstall dev_install_;
};

}  // namespace

// The --yes flag should pass w/out prompting the user.
TEST_F(PromptUserTest, Forced) {
  dev_install_.SetYesForTest(true);
  std::stringstream stream("");
  EXPECT_TRUE(dev_install_.PromptUser(stream, ""));
}

// EOF input should fail.
TEST_F(PromptUserTest, Eof) {
  std::stringstream stream("");
  EXPECT_FALSE(dev_install_.PromptUser(stream, ""));
}

// Default input (hitting enter) should fail.
TEST_F(PromptUserTest, Default) {
  std::stringstream stream("\n");
  EXPECT_FALSE(dev_install_.PromptUser(stream, ""));
}

// Entering "n" should fail.
TEST_F(PromptUserTest, No) {
  std::stringstream stream("n\n");
  EXPECT_FALSE(dev_install_.PromptUser(stream, ""));
}

// Entering "y" should pass.
TEST_F(PromptUserTest, Yes) {
  std::stringstream stream("y\n");
  EXPECT_TRUE(dev_install_.PromptUser(stream, ""));
}

}  // namespace dev_install
