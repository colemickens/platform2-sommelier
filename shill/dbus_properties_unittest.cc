// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "shill/dbus_properties.h"

using std::string;
using testing::Test;

namespace shill {

class DBusPropertiesTest : public Test {
};

TEST_F(DBusPropertiesTest, GetString) {
  static const char kTestProperty[] = "RandomKey";
  static const char kTestValue[] = "random-value";
  static const char kOldValue[] = "old-value";
  string value = kOldValue;
  DBusPropertiesMap props;
  EXPECT_FALSE(DBusProperties::GetString(props, kTestProperty, &value));
  EXPECT_EQ(kOldValue, value);
  props[kTestProperty].writer().append_string(kTestValue);
  EXPECT_TRUE(DBusProperties::GetString(props, kTestProperty, &value));
  EXPECT_EQ(kTestValue, value);
}

TEST_F(DBusPropertiesTest, GetUint32) {
  static const char kTestProperty[] = "AKey";
  const uint32 kTestValue = 35;
  const uint32 kOldValue = 74;
  uint32 value = kOldValue;
  DBusPropertiesMap props;
  EXPECT_FALSE(DBusProperties::GetUint32(props, kTestProperty, &value));
  EXPECT_EQ(kOldValue, value);
  props[kTestProperty].writer().append_uint32(kTestValue);
  EXPECT_TRUE(DBusProperties::GetUint32(props, kTestProperty, &value));
  EXPECT_EQ(kTestValue, value);
}

}  // namespace shill
