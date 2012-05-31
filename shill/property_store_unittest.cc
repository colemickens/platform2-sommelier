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

using std::map;
using std::string;
using std::vector;
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
const ::DBus::Variant PropertyStoreTest::kUint32V =
    DBusAdaptor::Uint32ToVariant(0);

PropertyStoreTest::PropertyStoreTest()
    : internal_error_(Error::GetName(Error::kInternalError)),
      invalid_args_(Error::GetName(Error::kInvalidArguments)),
      invalid_prop_(Error::GetName(Error::kInvalidProperty)),
      path_(dir_.CreateUniqueTempDir() ? dir_.path().value() : ""),
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

TEST_P(PropertyStoreTest, TestProperty) {
  // Ensure that an attempt to write unknown properties returns InvalidProperty.
  PropertyStore store;
  ::DBus::Error error;
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
           PropertyStoreTest::kUint32V));

template <typename T>
class PropertyStoreTypedTest : public PropertyStoreTest {
 protected:
  void RegisterProperty(
      PropertyStore &store, const string &name, T *storage);
};

typedef ::testing::Types<
  bool, int16, int32, KeyValueStore, string, Stringmap, Stringmaps, Strings,
  uint8, uint16, uint32> PropertyTypes;
TYPED_TEST_CASE(PropertyStoreTypedTest, PropertyTypes);

TYPED_TEST(PropertyStoreTypedTest, ClearProperty) {
  PropertyStore store;
  Error error;
  TypeParam property;
  RegisterProperty(store, "some property", &property);
  EXPECT_TRUE(store.ClearProperty("some property", &error));
}

template<> void PropertyStoreTypedTest<bool>::RegisterProperty(
    PropertyStore &store, const string &name, bool *storage) {
  store.RegisterBool(name, storage);
}

template<> void PropertyStoreTypedTest<int16>::RegisterProperty(
    PropertyStore &store, const string &name, int16 *storage) {
  store.RegisterInt16(name, storage);
}

template<> void PropertyStoreTypedTest<int32>::RegisterProperty(
    PropertyStore &store, const string &name, int32 *storage) {
  store.RegisterInt32(name, storage);
}

template<> void PropertyStoreTypedTest<KeyValueStore>::RegisterProperty(
    PropertyStore &store, const string &name, KeyValueStore *storage) {
  // We use |RegisterDerivedKeyValueStore|, because there is no non-derived
  // version. (And it's not clear that we'll need one, outside of this
  // test.)
  store.RegisterDerivedKeyValueStore(
      name, KeyValueStoreAccessor(
          new PropertyAccessor<KeyValueStore>(storage)));
}

template<> void PropertyStoreTypedTest<string>::RegisterProperty(
    PropertyStore &store, const string &name, string *storage) {
  store.RegisterString(name, storage);
}

template<> void PropertyStoreTypedTest<Stringmap>::RegisterProperty(
    PropertyStore &store, const string &name, Stringmap *storage) {
  store.RegisterStringmap(name, storage);
}

template<> void PropertyStoreTypedTest<Stringmaps>::RegisterProperty(
    PropertyStore &store, const string &name, Stringmaps *storage) {
  store.RegisterStringmaps(name, storage);
}

template<> void PropertyStoreTypedTest<Strings>::RegisterProperty(
    PropertyStore &store, const string &name, Strings *storage) {
  store.RegisterStrings(name, storage);
}

template<> void PropertyStoreTypedTest<uint8>::RegisterProperty(
    PropertyStore &store, const string &name, uint8 *storage) {
  store.RegisterUint8(name, storage);
}

template<> void PropertyStoreTypedTest<uint16>::RegisterProperty(
    PropertyStore &store, const string &name, uint16 *storage) {
  store.RegisterUint16(name, storage);
}

template<> void PropertyStoreTypedTest<uint32>::RegisterProperty(
    PropertyStore &store, const string &name, uint32 *storage) {
  store.RegisterUint32(name, storage);
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
  PropertyStore store;
  Error error;

  EXPECT_FALSE(store.ClearProperty("", &error));
  EXPECT_EQ(Error::kInvalidProperty, error.type());
}

TEST_F(PropertyStoreTest, SetStringmapsProperty) {
  PropertyStore store;
  ::DBus::Error error;
  EXPECT_FALSE(DBusAdaptor::SetProperty(
      &store, "", PropertyStoreTest::kStringmapsV, &error));
  EXPECT_EQ(internal_error(), error.name());
}

TEST_F(PropertyStoreTest, SetKeyValueStoreProperty) {
  PropertyStore store;
  ::DBus::Error error;
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
  }
}

}  // namespace shill
