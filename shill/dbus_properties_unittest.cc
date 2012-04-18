// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "shill/dbus_properties.h"

using std::string;
using std::vector;
using testing::Test;

namespace shill {

class DBusPropertiesTest : public Test {
};

TEST_F(DBusPropertiesTest, GetObjectPath) {
  static const char kTestProperty[] = "RandomKey";
  static const char kTestValue[] = "/random/path";
  static const char kOldValue[] = "/random/oldpath";
  string value = kOldValue;
  DBusPropertiesMap props;
  EXPECT_FALSE(DBusProperties::GetObjectPath(props, kTestProperty, &value));
  EXPECT_EQ(kOldValue, value);
  props[kTestProperty].writer().append_path(kTestValue);
  EXPECT_TRUE(DBusProperties::GetObjectPath(props, kTestProperty, &value));
  EXPECT_EQ(kTestValue, value);
}

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

TEST_F(DBusPropertiesTest, GetStrings) {
  static const char kTestProperty[] = "RandomKey";
  static const char kTestValue[] = "random-value";
  static const char kOldValue[] = "old-value";
  vector<string> test_values;
  test_values.push_back(kTestValue);
  test_values.push_back(kTestValue);

  vector<string> values;
  DBusPropertiesMap props;
  EXPECT_FALSE(DBusProperties::GetStrings(props, kTestProperty, &values));
  EXPECT_EQ(0, values.size());
  values.push_back(kOldValue);
  EXPECT_FALSE(DBusProperties::GetStrings(props, kTestProperty, &values));
  EXPECT_EQ(1, values.size());
  EXPECT_EQ(kOldValue, values[0]);
  ::DBus::Variant v;
  ::DBus::MessageIter writer = v.writer();
  writer << test_values;
  props[kTestProperty] = v;
  EXPECT_TRUE(DBusProperties::GetStrings(props, kTestProperty, &values));
  EXPECT_EQ(2, values.size());
  EXPECT_EQ(kTestValue, values[0]);
  EXPECT_EQ(kTestValue, values[1]);
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

TEST_F(DBusPropertiesTest, GetUint16) {
  static const char kTestProperty[] = "version";
  const uint16 kTestValue = 77;
  const uint16 kOldValue = 88;
  uint16 value = kOldValue;
  DBusPropertiesMap props;
  EXPECT_FALSE(DBusProperties::GetUint16(props, kTestProperty, &value));
  EXPECT_EQ(kOldValue, value);
  props[kTestProperty].writer().append_uint16(kTestValue);
  EXPECT_TRUE(DBusProperties::GetUint16(props, kTestProperty, &value));
  EXPECT_EQ(kTestValue, value);
}

}  // namespace shill
