// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_properties.h"

#include <limits>

#include <gtest/gtest.h>

using std::numeric_limits;
using std::string;
using std::vector;
using testing::Test;

namespace shill {

class DBusPropertiesTest : public Test {};

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
  static const char kStringsKey[] = "StringsKey";
  const vector<string> kStringsValue = {"StringsValue1", "StringsValue2"};
  static const char kBoolKey[] = "BoolKey";
  const bool kBoolValue = true;
  static const char kInt32Key[] = "Int32Key";
  const int32 kInt32Value = 123;
  static const char kUint32Key[] = "Uint32Key";
  const uint32 kUint32Value = 654;
  KeyValueStore store;
  store.SetString(kStringKey, kStringValue);
  store.SetStrings(kStringsKey, kStringsValue);
  store.SetBool(kBoolKey, kBoolValue);
  store.SetInt(kInt32Key, kInt32Value);
  store.SetUint(kUint32Key, kUint32Value);
  DBusPropertiesMap props;
  props["RandomKey"].writer().append_string("RandomValue");
  DBusProperties::ConvertKeyValueStoreToMap(store, &props);
  EXPECT_EQ(5, props.size());
  string string_value;
  EXPECT_TRUE(DBusProperties::GetString(props, kStringKey, &string_value));
  EXPECT_EQ(kStringValue, string_value);
  vector<string> strings_value;
  EXPECT_TRUE(DBusProperties::GetStrings(props, kStringsKey, &strings_value));
  EXPECT_EQ(kStringsValue, strings_value);
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

template <typename T> class DBusPropertiesGetterTest : public Test {};

template <typename DerivedT, typename ValueT, typename DBusT = ValueT>
struct TestTraits {
  typedef ValueT ValueType;
  typedef DBusT DBusType;
  typedef bool (*GetterType)(const DBusPropertiesMap &properties,
                             const string &key,
                             ValueType *value);

  static DBusType GetTestValue() { return DerivedT::GetNewValue(); }

  static void CheckValueEqual(const ValueType &expected,
                              const ValueType &actual) {
    EXPECT_EQ(expected, actual);
  }
};

struct BoolTestTraits : public TestTraits<BoolTestTraits, bool> {
  static bool GetOldValue() { return false; }
  static bool GetNewValue() { return true; }
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetBool;
};

template <typename DerivedT, typename ValueT>
struct IntTestTraits : public TestTraits<DerivedT, ValueT> {
  static ValueT GetOldValue() { return numeric_limits<ValueT>::min(); }
  static ValueT GetNewValue() { return numeric_limits<ValueT>::max(); }
};

struct Int16TestTraits : public IntTestTraits<Int16TestTraits, int16> {
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetInt16;
};

struct Int32TestTraits : public IntTestTraits<Int32TestTraits, int32> {
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetInt32;
};

struct Int64TestTraits : public IntTestTraits<Int64TestTraits, int64> {
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetInt64;
};

struct Uint8TestTraits : public IntTestTraits<Uint8TestTraits, uint8> {
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetUint8;
};

struct Uint16TestTraits : public IntTestTraits<Uint16TestTraits, uint16> {
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetUint16;
};

struct Uint32TestTraits : public IntTestTraits<Uint32TestTraits, uint32> {
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetUint32;
};

struct Uint64TestTraits : public IntTestTraits<Uint64TestTraits, uint64> {
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetUint64;
};

struct DoubleTestTraits : public TestTraits<DoubleTestTraits, double> {
  static double GetOldValue() { return -1.1; }
  static double GetNewValue() { return 3.14; }
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetDouble;
};

struct StringTestTraits : public TestTraits<StringTestTraits, string> {
  static string GetOldValue() { return "old"; }
  static string GetNewValue() { return "new"; }
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetString;
};

struct StringsTestTraits
    : public TestTraits<StringsTestTraits, vector<string>> {
  static vector<string> GetOldValue() { return {"1", "2", "3"}; }
  static vector<string> GetNewValue() { return {"a", "b"}; }
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetStrings;
};

struct ObjectPathTestTraits
    : public TestTraits<ObjectPathTestTraits, DBus::Path> {
  static DBus::Path GetOldValue() { return "/old/path"; }
  static DBus::Path GetNewValue() { return "/new/path"; }
  static constexpr GetterType kMethodUnderTest = &DBusProperties::GetObjectPath;
};

struct RpcIdentifiersTestTraits : public TestTraits<RpcIdentifiersTestTraits,
                                                    RpcIdentifiers,
                                                    vector<DBus::Path>> {
  static RpcIdentifiers GetOldValue() { return {"/device/1", "/service/2"}; }
  static RpcIdentifiers GetNewValue() { return {"/obj/3", "/obj/4", "/obj/5"}; }
  static vector<DBus::Path> GetTestValue() {
    return {"/obj/3", "/obj/4", "/obj/5"};
  }
  static constexpr GetterType kMethodUnderTest =
      &DBusProperties::GetRpcIdentifiers;
};

struct DBusPropertiesMapTestTraits
    : public TestTraits<DBusPropertiesMapTestTraits, DBusPropertiesMap> {
  static DBusPropertiesMap GetOldValue() {
    DBusPropertiesMap value;
    value["OldKey"].writer().append_bool(false);
    return value;
  }

  static DBusPropertiesMap GetNewValue() {
    DBusPropertiesMap value;
    value["BoolKey"].writer().append_bool(true);
    value["Int16Key"].writer().append_int16(numeric_limits<int16>::max());
    value["Int32Key"].writer().append_int32(numeric_limits<int32>::max());
    value["Int64Key"].writer().append_int64(numeric_limits<int64>::max());
    value["Uint8Key"].writer().append_byte(numeric_limits<uint8>::max());
    value["Uint16Key"].writer().append_uint16(numeric_limits<uint16>::max());
    value["Uint32Key"].writer().append_uint32(numeric_limits<uint32>::max());
    value["Uint64Key"].writer().append_uint64(numeric_limits<uint64>::max());
    value["DoubleKey"].writer().append_double(3.14);
    value["StringKey"].writer().append_string("new");
    return value;
  }

  template <typename T>
  static void CheckDBusVariantEqual(const string &key,
                                    const DBus::Variant &expected,
                                    const DBus::Variant &actual) {
    T expected_value = expected.operator T();
    T actual_value = actual.operator T();
    EXPECT_EQ(expected_value, actual_value) << "Value mismatch - key: " << key;
  }

  static void CheckValueEqual(const DBusPropertiesMap &expected,
                              const DBusPropertiesMap &actual) {
    ASSERT_EQ(expected.size(), actual.size()) << "Map size mismatch";

    for (const auto &key_value_pair : expected) {
      const string &key = key_value_pair.first;

      const auto &actual_it = actual.find(key);
      ASSERT_TRUE(actual_it != actual.end()) << "Key '" << key << "' not found";

      const DBus::Variant &actual_value = actual_it->second;
      const DBus::Variant &expected_value = key_value_pair.second;

      string actual_signature = actual_value.signature();
      string expected_signature = expected_value.signature();
      ASSERT_EQ(expected_signature, actual_signature)
          << "Value type mismatch - key: " << key;

      if (expected_signature == DBus::type<bool>::sig()) {
        CheckDBusVariantEqual<bool>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<int16>::sig()) {
        CheckDBusVariantEqual<int16>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<int32>::sig()) {
        CheckDBusVariantEqual<int32>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<int64>::sig()) {
        CheckDBusVariantEqual<int64>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<uint8>::sig()) {
        CheckDBusVariantEqual<uint8>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<uint16>::sig()) {
        CheckDBusVariantEqual<uint16>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<uint32>::sig()) {
        CheckDBusVariantEqual<uint32>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<uint64>::sig()) {
        CheckDBusVariantEqual<uint64>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<double>::sig()) {
        CheckDBusVariantEqual<double>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<string>::sig()) {
        CheckDBusVariantEqual<string>(key, expected_value, actual_value);
      } else if (expected_signature == DBus::type<DBus::Path>::sig()) {
        CheckDBusVariantEqual<DBus::Path>(key, expected_value, actual_value);
      } else {
        // TODO(benchan): Comparison of other types is not yet implemented.
        FAIL() << "Value type '" << expected_signature << "' not implemented.";
      }
    }
  }

  static constexpr GetterType kMethodUnderTest =
      &DBusProperties::GetDBusPropertiesMap;
};

typedef testing::Types<BoolTestTraits,
                       Int16TestTraits,
                       Int32TestTraits,
                       Int64TestTraits,
                       Uint8TestTraits,
                       Uint16TestTraits,
                       Uint32TestTraits,
                       Uint64TestTraits,
                       DoubleTestTraits,
                       StringTestTraits,
                       StringsTestTraits,
                       ObjectPathTestTraits,
                       RpcIdentifiersTestTraits,
                       DBusPropertiesMapTestTraits> DBusPropertyValueTypes;
TYPED_TEST_CASE(DBusPropertiesGetterTest, DBusPropertyValueTypes);

TYPED_TEST(DBusPropertiesGetterTest, GetValue) {
  static const char kTestProperty[] = "TestProperty";
  const typename TypeParam::ValueType kOldValue = TypeParam::GetOldValue();
  const typename TypeParam::ValueType kNewValue = TypeParam::GetNewValue();
  const typename TypeParam::DBusType kTestValue = TypeParam::GetTestValue();
  typename TypeParam::ValueType value = kOldValue;
  DBusPropertiesMap properties;

  // Property key is not found. |value| should remain the initial value.
  EXPECT_FALSE(TypeParam::kMethodUnderTest(properties, kTestProperty, &value));
  TypeParam::CheckValueEqual(kOldValue, value);

  // Property value type mismatch. |value| should remain the initial value.
  properties[kTestProperty].writer().append_fd(1);
  EXPECT_FALSE(TypeParam::kMethodUnderTest(properties, kTestProperty, &value));
  TypeParam::CheckValueEqual(kOldValue, value);

  // Property key is found. |value| should be set to the test value.
  properties.clear();
  DBus::MessageIter writer = properties[kTestProperty].writer();
  writer << kTestValue;
  EXPECT_TRUE(TypeParam::kMethodUnderTest(properties, kTestProperty, &value));
  TypeParam::CheckValueEqual(kNewValue, value);
}

}  // namespace shill
