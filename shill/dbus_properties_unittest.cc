// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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

TEST_F(DBusPropertiesTest, GetRpcIdentifiers) {
  static const char kTestProperty[] = "paths";
  static const char kOldTestPath0[] = "/org/chromium/Something/Old";
  static const char kOldTestPath1[] = "/org/chromium/Else/Old";
  static const char kTestPath0[] = "/org/chromium/Something";
  static const char kTestPath1[] = "/org/chromium/Else";

  RpcIdentifiers value;
  value.push_back(kOldTestPath0);
  value.push_back(kOldTestPath1);
  DBusPropertiesMap props;
  EXPECT_FALSE(DBusProperties::GetRpcIdentifiers(props, kTestProperty, &value));
  ASSERT_EQ(2, value.size());
  EXPECT_EQ(kOldTestPath0, value[0]);
  EXPECT_EQ(kOldTestPath1, value[1]);

  vector<DBus::Path> paths;
  paths.push_back(kTestPath0);
  paths.push_back(kTestPath1);
  DBus::MessageIter writer = props[kTestProperty].writer();
  writer << paths;
  EXPECT_TRUE(DBusProperties::GetRpcIdentifiers(props, kTestProperty, &value));
  ASSERT_EQ(2, value.size());
  EXPECT_EQ(kTestPath0, value[0]);
  EXPECT_EQ(kTestPath1, value[1]);
}

TEST_F(DBusPropertiesTest, ConvertPathsToRpcIdentifiers) {
  static const char kOldTestPath[] = "/org/chromium/Something/Old";
  static const char kTestPath0[] = "/org/chromium/Something";
  static const char kTestPath1[] = "/org/chromium/Else";

  RpcIdentifiers ids(1, kOldTestPath);
  vector<DBus::Path> paths;
  paths.push_back(kTestPath0);
  paths.push_back(kTestPath1);
  DBusProperties::ConvertPathsToRpcIdentifiers(paths, &ids);
  ASSERT_EQ(2, ids.size());
  EXPECT_EQ(kTestPath0, ids[0]);
  EXPECT_EQ(kTestPath1, ids[1]);
}

TEST_F(DBusPropertiesTest, ConvertKeyValueStoreToMap) {
  static const char kStringKey[] = "StringKey";
  static const char kStringValue[] = "StringValue";
  static const char kBoolKey[] = "BoolKey";
  const bool kBoolValue = true;
  static const char kInt32Key[] = "Int32Key";
  const int32 kInt32Value = 123;
  static const char kUint32Key[] = "Uint32Key";
  const uint32 kUint32Value = 654;
  KeyValueStore store;
  store.SetString(kStringKey, kStringValue);
  store.SetBool(kBoolKey, kBoolValue);
  store.SetInt(kInt32Key, kInt32Value);
  store.SetUint(kUint32Key, kUint32Value);
  DBusPropertiesMap props;
  props["RandomKey"].writer().append_string("RandomValue");
  DBusProperties::ConvertKeyValueStoreToMap(store, &props);
  EXPECT_EQ(4, props.size());
  string string_value;
  EXPECT_TRUE(DBusProperties::GetString(props, kStringKey, &string_value));
  EXPECT_EQ(kStringValue, string_value);
  bool bool_value = !kBoolValue;
  EXPECT_TRUE(DBusProperties::GetBool(props, kBoolKey, &bool_value));
  EXPECT_EQ(kBoolValue, bool_value);
  int32 int32_value = ~kInt32Value;
  EXPECT_TRUE(DBusProperties::GetInt32(props, kInt32Key, &int32_value));
  EXPECT_EQ(kInt32Value, int32_value);
  uint32 uint32_value = ~kUint32Value;
  EXPECT_TRUE(DBusProperties::GetUint32(props, kUint32Key, &uint32_value));
  EXPECT_EQ(kUint32Value, uint32_value);
}

}  // namespace shill
