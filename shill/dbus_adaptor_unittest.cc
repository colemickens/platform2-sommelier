// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_dbus_adaptor.h"

#include <string>

#include <dbus-c++/dbus.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace shill {
using ::testing::Test;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class DBusAdaptorTest : public Test {
 public:
  DBusAdaptorTest() {}
  virtual ~DBusAdaptorTest() {}
};

TEST_F(DBusAdaptorTest, Conversions) {
  bool expected = false;
  EXPECT_EQ(expected, DBusAdaptor::BoolToVariant(expected).reader().get_bool());
  expected = true;
  EXPECT_EQ(expected, DBusAdaptor::BoolToVariant(expected).reader().get_bool());

  uint32 ex_uint = 0;
  EXPECT_EQ(ex_uint,
            DBusAdaptor::UInt32ToVariant(ex_uint).reader().get_uint32());
  ex_uint = 128;
  EXPECT_EQ(ex_uint,
            DBusAdaptor::UInt32ToVariant(ex_uint).reader().get_uint32());

  int32 ex_int = 0;
  EXPECT_EQ(ex_int,
            DBusAdaptor::IntToVariant(ex_int).reader().get_int32());
  ex_int = 128;
  EXPECT_EQ(ex_int,
            DBusAdaptor::IntToVariant(ex_int).reader().get_int32());

  std::string ex_string = "";
  EXPECT_EQ(ex_string,
            DBusAdaptor::StringToVariant(ex_string).reader().get_string());
  ex_string = "hello";
  EXPECT_EQ(ex_string,
            DBusAdaptor::StringToVariant(ex_string).reader().get_string());
}

}  // namespace shill
