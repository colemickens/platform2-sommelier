// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_store_unittest.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/dbus_adaptor.h"
#include "shill/error.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/property_accessor.h"
#include "shill/property_store.h"

using base::Bind;
using base::Unretained;
using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::Values;

namespace shill {

// static
const ::DBus::Variant PropertyStoreTest::kBoolV = DBusAdaptor::BoolToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kByteV = DBusAdaptor::ByteToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kInt16V =
    DBusAdaptor::Int16ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kInt32V =
    DBusAdaptor::Int32ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kKeyValueStoreV =
    DBusAdaptor::KeyValueStoreToVariant(KeyValueStore());
// static
const ::DBus::Variant PropertyStoreTest::kStringV =
    DBusAdaptor::StringToVariant("");
// static
const ::DBus::Variant PropertyStoreTest::kStringmapV =
    DBusAdaptor::StringmapToVariant(Stringmap());
// static
const ::DBus::Variant PropertyStoreTest::kStringmapsV =
    DBusAdaptor::StringmapsToVariant(Stringmaps());
// static
const ::DBus::Variant PropertyStoreTest::kStringsV =
    DBusAdaptor::StringsToVariant(Strings(1, ""));
// static
const ::DBus::Variant PropertyStoreTest::kUint16V =
    DBusAdaptor::Uint16ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kUint16sV =
    DBusAdaptor::Uint16sToVariant(Uint16s{0});
// static
const ::DBus::Variant PropertyStoreTest::kUint32V =
    DBusAdaptor::Uint32ToVariant(0);
// static
const ::DBus::Variant PropertyStoreTest::kUint64V =
    DBusAdaptor::Uint64ToVariant(0);

PropertyStoreTest::PropertyStoreTest()
    : internal_error_(Error::GetName(Error::kInternalError)),
      invalid_args_(Error::GetName(Error::kInvalidArguments)),
      invalid_prop_(Error::GetName(Error::kInvalidProperty)),
      path_(dir_.CreateUniqueTempDir() ? dir_.path().value() : ""),
      metrics_(dispatcher()),
      manager_(control_interface(),
               dispatcher(),
               metrics(),
               glib(),
               run_path(),
               storage_path(),
               string()) {
}

PropertyStoreTest::~PropertyStoreTest() {}

void PropertyStoreTest::SetUp() {
  ASSERT_FALSE(run_path().empty());
  ASSERT_FALSE(storage_path().empty());
}

TEST_P(PropertyStoreTest, SetPropertyNonexistent) {
  // Ensure that an attempt to write unknown properties returns
  // InvalidProperty, and does not yield a PropertyChange callback.
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  ::DBus::Error error;
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", GetParam(), &error));
  EXPECT_EQ(invalid_prop(), error.name());
}

INSTANTIATE_TEST_CASE_P(
    PropertyStoreTestInstance,
    PropertyStoreTest,
    Values(PropertyStoreTest::kBoolV,
           PropertyStoreTest::kByteV,
           PropertyStoreTest::kInt16V,
           PropertyStoreTest::kInt32V,
           PropertyStoreTest::kStringV,
           PropertyStoreTest::kStringmapV,
           PropertyStoreTest::kStringsV,
           PropertyStoreTest::kUint16V,
           PropertyStoreTest::kUint16sV,
           PropertyStoreTest::kUint32V,
           PropertyStoreTest::kUint64V));

template <typename T>
class PropertyStoreTypedTest : public PropertyStoreTest {
 protected:
  bool SetProperty(
      PropertyStore &store, const string &name, Error *error);
};

TYPED_TEST_CASE(PropertyStoreTypedTest, PropertyStoreTest::PropertyTypes);

TYPED_TEST(PropertyStoreTypedTest, RegisterProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;
  TypeParam property;
  PropertyStoreTest::RegisterProperty(&store, "some property", &property);
  EXPECT_TRUE(store.Contains("some property"));
}

TYPED_TEST(PropertyStoreTypedTest, GetProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;
  TypeParam property{};  // value-initialize primitives
  PropertyStoreTest::RegisterProperty(&store, "some property", &property);

  TypeParam read_value;
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_TRUE(PropertyStoreTest::GetProperty(
      store, "some property", &read_value, &error));
  EXPECT_EQ(property, read_value);
}

TYPED_TEST(PropertyStoreTypedTest, ClearProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;
  TypeParam property;
  PropertyStoreTest::RegisterProperty(&store, "some property", &property);
  EXPECT_CALL(*this, TestCallback(_));
  EXPECT_TRUE(store.ClearProperty("some property", &error));
}

TYPED_TEST(PropertyStoreTypedTest, SetProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;
  TypeParam property{};  // value-initialize primitives
  PropertyStoreTest::RegisterProperty(&store, "some property", &property);

  // Change the value from the default (initialized above).  Should
  // generate a change callback. The second SetProperty, however,
  // should not. Hence, we should get exactly one callback.
  EXPECT_CALL(*this, TestCallback(_)).Times(1);
  EXPECT_TRUE(this->SetProperty(store, "some property", &error));
  EXPECT_FALSE(this->SetProperty(store, "some property", &error));
}

template<> bool PropertyStoreTypedTest<bool>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  bool new_value = true;
  return store.SetBoolProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<int16>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  int16 new_value = 1;
  return store.SetInt16Property(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<int32>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  int32 new_value = 1;
  return store.SetInt32Property(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<string>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  string new_value = "new value";
  return store.SetStringProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<Stringmap>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  Stringmap new_value;
  new_value["new key"] = "new value";
  return store.SetStringmapProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<Stringmaps>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  Stringmaps new_value(1);
  new_value[0]["new key"] = "new value";
  return store.SetStringmapsProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<Strings>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  Strings new_value(1);
  new_value[0] = "new value";
  return store.SetStringsProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<uint8>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  uint8 new_value = 1;
  return store.SetUint8Property(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<uint16>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  uint16 new_value = 1;
  return store.SetUint16Property(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<Uint16s>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  Uint16s new_value{1};
  return store.SetUint16sProperty(name, new_value, error);
}

template<> bool PropertyStoreTypedTest<uint32>::SetProperty(
    PropertyStore &store, const string &name, Error *error) {
  uint32 new_value = 1;
  return store.SetUint32Property(name, new_value, error);
}

TEST_F(PropertyStoreTest, ClearBoolProperty) {
  // We exercise both possibilities for the default value here,
  // to ensure that Clear actually resets the property based on
  // the property's initial value (rather than the language's
  // default value for the type).
  static const bool kDefaults[] = {true, false};
  for (size_t i = 0; i < arraysize(kDefaults); ++i) {
    PropertyStore store;
    Error error;

    const bool default_value = kDefaults[i];
    bool flag = default_value;
    store.RegisterBool("some bool", &flag);

    EXPECT_TRUE(store.ClearProperty("some bool", &error));
    EXPECT_EQ(default_value, flag);
  }
}

TEST_F(PropertyStoreTest, ClearPropertyNonexistent) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  Error error;

  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_FALSE(store.ClearProperty("", &error));
  EXPECT_EQ(Error::kInvalidProperty, error.type());
}

// Separate from SetPropertyNonexistent, because
// DBusAdaptor::SetProperty doesn't support Stringmaps.
TEST_F(PropertyStoreTest, SetStringmapsProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  ::DBus::Error error;
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_FALSE(DBusAdaptor::SetProperty(
      &store, "", PropertyStoreTest::kStringmapsV, &error));
  EXPECT_EQ(internal_error(), error.name());
}

// Separate from SetPropertyNonexistent, because
// DBusAdaptor::SetProperty doesn't support KeyValueStore.
TEST_F(PropertyStoreTest, SetKeyValueStoreProperty) {
  PropertyStore store(Bind(&PropertyStoreTest::TestCallback,
                           Unretained(this)));
  ::DBus::Error error;
  EXPECT_CALL(*this, TestCallback(_)).Times(0);
  EXPECT_FALSE(DBusAdaptor::SetProperty(
      &store, "", PropertyStoreTest::kKeyValueStoreV, &error));
  EXPECT_EQ(internal_error(), error.name());
}

TEST_F(PropertyStoreTest, WriteOnlyProperties) {
  // Test that properties registered as write-only are not returned
  // when using Get*PropertiesIter().
  PropertyStore store;
  {
    const string keys[]  = {"boolp1", "boolp2"};
    bool values[] = {true, true};
    store.RegisterWriteOnlyBool(keys[0], &values[0]);
    store.RegisterBool(keys[1], &values[1]);

    ReadablePropertyConstIterator<bool> it = store.GetBoolPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_TRUE(values[1] == it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetBoolProperty(keys[0], NULL, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    bool test_value;
    EXPECT_TRUE(store.GetBoolProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"int16p1", "int16p2"};
    int16 values[] = {127, 128};
    store.RegisterWriteOnlyInt16(keys[0], &values[0]);
    store.RegisterInt16(keys[1], &values[1]);

    ReadablePropertyConstIterator<int16> it = store.GetInt16PropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetInt16Property(keys[0], NULL, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    int16 test_value;
    EXPECT_TRUE(store.GetInt16Property(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"int32p1", "int32p2"};
    int32 values[] = {127, 128};
    store.RegisterWriteOnlyInt32(keys[0], &values[0]);
    store.RegisterInt32(keys[1], &values[1]);

    ReadablePropertyConstIterator<int32> it = store.GetInt32PropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetInt32Property(keys[0], NULL, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    int32 test_value;
    EXPECT_TRUE(store.GetInt32Property(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"stringp1", "stringp2"};
    string values[] = {"noooo", "yesss"};
    store.RegisterWriteOnlyString(keys[0], &values[0]);
    store.RegisterString(keys[1], &values[1]);

    ReadablePropertyConstIterator<string> it = store.GetStringPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetStringProperty(keys[0], NULL, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    string test_value;
    EXPECT_TRUE(store.GetStringProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"stringmapp1", "stringmapp2"};
    Stringmap values[2];
    values[0]["noooo"] = "yesss";
    values[1]["yesss"] = "noooo";
    store.RegisterWriteOnlyStringmap(keys[0], &values[0]);
    store.RegisterStringmap(keys[1], &values[1]);

    ReadablePropertyConstIterator<Stringmap> it =
        store.GetStringmapPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_TRUE(values[1] == it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetStringmapProperty(keys[0], NULL, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    Stringmap test_value;
    EXPECT_TRUE(store.GetStringmapProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_TRUE(values[1] == test_value);
  }
  {
    const string keys[] = {"stringmapsp1", "stringmapsp2"};
    Stringmaps values[2];
    Stringmap element;
    element["noooo"] = "yesss";
    values[0].push_back(element);
    element["yesss"] = "noooo";
    values[1].push_back(element);

    store.RegisterWriteOnlyStringmaps(keys[0], &values[0]);
    store.RegisterStringmaps(keys[1], &values[1]);

    ReadablePropertyConstIterator<Stringmaps> it =
        store.GetStringmapsPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_TRUE(values[1] == it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetStringmapsProperty(keys[0], NULL, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    Stringmaps test_value;
    EXPECT_TRUE(store.GetStringmapsProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_TRUE(values[1] == test_value);
  }
  {
    const string keys[] = {"stringsp1", "stringsp2"};
    Strings values[2];
    string element;
    element = "noooo";
    values[0].push_back(element);
    element = "yesss";
    values[1].push_back(element);
    store.RegisterWriteOnlyStrings(keys[0], &values[0]);
    store.RegisterStrings(keys[1], &values[1]);

    ReadablePropertyConstIterator<Strings> it =
        store.GetStringsPropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_TRUE(values[1] == it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetStringsProperty(keys[0], NULL, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    Strings test_value;
    EXPECT_TRUE(store.GetStringsProperty(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_TRUE(values[1] == test_value);
  }
  {
    const string keys[] = {"uint8p1", "uint8p2"};
    uint8 values[] = {127, 128};
    store.RegisterWriteOnlyUint8(keys[0], &values[0]);
    store.RegisterUint8(keys[1], &values[1]);

    ReadablePropertyConstIterator<uint8> it = store.GetUint8PropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetUint8Property(keys[0], NULL, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    uint8 test_value;
    EXPECT_TRUE(store.GetUint8Property(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
  {
    const string keys[] = {"uint16p", "uint16p1"};
    uint16 values[] = {127, 128};
    store.RegisterWriteOnlyUint16(keys[0], &values[0]);
    store.RegisterUint16(keys[1], &values[1]);

    ReadablePropertyConstIterator<uint16> it = store.GetUint16PropertiesIter();
    EXPECT_FALSE(it.AtEnd());
    EXPECT_EQ(keys[1], it.Key());
    EXPECT_EQ(values[1], it.value());
    it.Advance();
    EXPECT_TRUE(it.AtEnd());

    Error errors[2];
    EXPECT_FALSE(store.GetUint16Property(keys[0], NULL, &errors[0]));
    EXPECT_EQ(Error::kPermissionDenied, errors[0].type());
    uint16 test_value;
    EXPECT_TRUE(store.GetUint16Property(keys[1], &test_value, &errors[1]));
    EXPECT_TRUE(errors[1].IsSuccess());
    EXPECT_EQ(values[1], test_value);
  }
}

}  // namespace shill
