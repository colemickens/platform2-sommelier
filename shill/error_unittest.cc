// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/error.h"

#include <dbus-c++/error.h>
#include <gtest/gtest.h>

#include "shill/dbus_adaptor.h"

using testing::Test;

namespace shill {

class ErrorTest : public Test {};

TEST_F(ErrorTest, ConstructorDefault) {
  Error e;
  EXPECT_EQ(Error::kSuccess, e.type());
  EXPECT_EQ(Error::GetDefaultMessage(Error::kSuccess), e.message());
}

TEST_F(ErrorTest, ConstructorDefaultMessage) {
  Error e(Error::kAlreadyExists);
  EXPECT_EQ(Error::kAlreadyExists, e.type());
  EXPECT_EQ(Error::GetDefaultMessage(Error::kAlreadyExists), e.message());
}

TEST_F(ErrorTest, ConstructorCustomMessage) {
  static const char kMessage[] = "Custom error message";
  Error e(Error::kInProgress, kMessage);
  EXPECT_EQ(Error::kInProgress, e.type());
  EXPECT_EQ(kMessage, e.message());
}

TEST_F(ErrorTest, Reset) {
  Error e(Error::kAlreadyExists);
  e.Reset();
  EXPECT_EQ(Error::kSuccess, e.type());
  EXPECT_EQ(Error::GetDefaultMessage(Error::kSuccess), e.message());
}

TEST_F(ErrorTest, PopulateDefaultMessage) {
  Error e;
  e.Populate(Error::kInternalError);
  EXPECT_EQ(Error::kInternalError, e.type());
  EXPECT_EQ(Error::GetDefaultMessage(Error::kInternalError), e.message());
}

TEST_F(ErrorTest, PopulateCustomMessage) {
  static const char kMessage[] = "Another custom error message";
  Error e;
  e.Populate(Error::kInvalidArguments, kMessage);
  EXPECT_EQ(Error::kInvalidArguments, e.type());
  EXPECT_EQ(kMessage, e.message());
}

TEST_F(ErrorTest, ToDBusError) {
  DBus::Error dbus_error;
  ASSERT_FALSE(dbus_error.is_set());
  Error().ToDBusError(&dbus_error);
  ASSERT_FALSE(dbus_error.is_set());
  static const char kMessage[] = "Test error message";
  Error(Error::kPermissionDenied, kMessage).ToDBusError(&dbus_error);
  ASSERT_TRUE(dbus_error.is_set());
  EXPECT_EQ(Error::GetName(Error::kPermissionDenied), dbus_error.name());
  EXPECT_STREQ(kMessage, dbus_error.message());
}

TEST_F(ErrorTest, IsSuccessFailure) {
  EXPECT_TRUE(Error().IsSuccess());
  EXPECT_FALSE(Error().IsFailure());
  EXPECT_FALSE(Error(Error::kInvalidNetworkName).IsSuccess());
  EXPECT_TRUE(Error(Error::kInvalidPassphrase).IsFailure());
}

TEST_F(ErrorTest, GetName) {
  EXPECT_EQ(SHILL_INTERFACE ".Error.NotFound",
            Error::GetName(Error::kNotFound));
}

TEST_F(ErrorTest, GetDefaultMessage) {
  // Check the last error code to try to prevent off-by-one bugs when adding or
  // removing error types.
  ASSERT_EQ(Error::kPermissionDenied, Error::kNumErrors - 1);
  EXPECT_EQ("Permission denied",
            Error::GetDefaultMessage(Error::kPermissionDenied));
}

}  // namespace shill
