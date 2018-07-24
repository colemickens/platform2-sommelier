// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/event_dispatcher.h"
#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_glib.h"
#include "shill/mock_profile.h"
#include "shill/mock_property_store.h"
#include "shill/mock_service.h"
#include "shill/property_store_unittest.h"

using std::map;
using std::string;
using std::vector;
using ::testing::_;
using ::testing::ContainerEq;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Test;

namespace shill {

class DBusAdaptorTest : public PropertyStoreTest {
 public:
  DBusAdaptorTest()
      : ex_bool_(true),
        ex_byte_(0xff),
        ex_bytearrays_{ByteArray()},
        ex_uint16_(65535),
        ex_uint16s_{ex_uint16_},
        ex_uint32_(2000000),
        ex_uint64_(8589934591LL),
        ex_int16_(-32768),
        ex_int32_(-65536),
        ex_path_("/"),
        ex_paths_{ex_path_},
        ex_string_("something"),
        ex_stringmap_{{ex_string_, ex_string_}},
        ex_stringmaps_{ex_stringmap_},
        ex_strings_(1, ex_string_),
        bool_v_(DBusAdaptor::BoolToVariant(ex_bool_)),
        byte_v_(DBusAdaptor::ByteToVariant(ex_byte_)),
        bytearrays_v_(DBusAdaptor::ByteArraysToVariant(ex_bytearrays_)),
        int16_v_(DBusAdaptor::Int16ToVariant(ex_int16_)),
        int32_v_(DBusAdaptor::Int32ToVariant(ex_int32_)),
        path_v_(DBusAdaptor::PathToVariant(ex_path_)),
        paths_v_(DBusAdaptor::PathsToVariant(ex_paths_)),
        string_v_(DBusAdaptor::StringToVariant(ex_string_)),
        stringmap_v_(DBusAdaptor::StringmapToVariant(ex_stringmap_)),
        stringmaps_v_(DBusAdaptor::StringmapsToVariant(ex_stringmaps_)),
        strings_v_(DBusAdaptor::StringsToVariant(ex_strings_)),
        uint16_v_(DBusAdaptor::Uint16ToVariant(ex_uint16_)),
        uint16s_v_(DBusAdaptor::Uint16sToVariant(ex_uint16s_)),
        uint32_v_(DBusAdaptor::Uint32ToVariant(ex_uint32_)),
        uint64_v_(DBusAdaptor::Uint64ToVariant(ex_uint64_)),
        device_(new MockDevice(control_interface(),
                               dispatcher(),
                               metrics(),
                               manager(),
                               "mock",
                               "addr0",
                               0)),
        service_(new MockService(control_interface(),
                                 dispatcher(),
                                 metrics(),
                                 manager())) {}

  virtual ~DBusAdaptorTest() {}

 protected:
  bool ex_bool_;
  uint8_t ex_byte_;
  ByteArrays ex_bytearrays_;
  uint16_t ex_uint16_;
  vector<uint16_t> ex_uint16s_;
  uint32_t ex_uint32_;
  uint64_t ex_uint64_;
  int16_t ex_int16_;
  int32_t ex_int32_;
  ::DBus::Path ex_path_;
  vector<::DBus::Path> ex_paths_;
  string ex_string_;
  map<string, string> ex_stringmap_;
  vector<map<string, string>> ex_stringmaps_;
  vector<string> ex_strings_;

  ::DBus::Variant bool_v_;
  ::DBus::Variant byte_v_;
  ::DBus::Variant bytearrays_v_;
  ::DBus::Variant int16_v_;
  ::DBus::Variant int32_v_;
  ::DBus::Variant path_v_;
  ::DBus::Variant paths_v_;
  ::DBus::Variant string_v_;
  ::DBus::Variant stringmap_v_;
  ::DBus::Variant stringmaps_v_;
  ::DBus::Variant strings_v_;
  ::DBus::Variant uint16_v_;
  ::DBus::Variant uint16s_v_;
  ::DBus::Variant uint32_v_;
  ::DBus::Variant uint64_v_;

  DeviceRefPtr device_;
  ServiceRefPtr service_;
};

TEST_F(DBusAdaptorTest, Conversions) {
  EXPECT_EQ(0, PropertyStoreTest::kBoolV.reader().get_bool());
  EXPECT_EQ(ex_bool_, bool_v_.reader().get_bool());

  EXPECT_EQ(0, PropertyStoreTest::kByteV.reader().get_byte());
  EXPECT_EQ(ex_byte_, byte_v_.reader().get_byte());

  EXPECT_TRUE(ex_bytearrays_ == bytearrays_v_.operator ByteArrays());

  EXPECT_EQ(0, PropertyStoreTest::kUint16V.reader().get_uint16());
  EXPECT_EQ(ex_uint16_, uint16_v_.reader().get_uint16());

  EXPECT_EQ(0, PropertyStoreTest::kInt16V.reader().get_int16());
  EXPECT_EQ(ex_int16_, int16_v_.reader().get_int16());

  EXPECT_EQ(0, PropertyStoreTest::kUint32V.reader().get_uint32());
  EXPECT_EQ(ex_uint32_, uint32_v_.reader().get_uint32());

  EXPECT_EQ(0, PropertyStoreTest::kUint64V.reader().get_uint64());
  EXPECT_EQ(ex_uint64_, uint64_v_.reader().get_uint64());

  EXPECT_EQ(0, PropertyStoreTest::kInt32V.reader().get_int32());
  EXPECT_EQ(ex_int32_, int32_v_.reader().get_int32());

  EXPECT_EQ(ex_path_, path_v_.reader().get_path());

  EXPECT_EQ(ex_path_, paths_v_.operator vector<::DBus::Path>()[0]);

  EXPECT_EQ(string(""), PropertyStoreTest::kStringV.reader().get_string());
  EXPECT_EQ(ex_string_, string_v_.reader().get_string());

  EXPECT_THAT(ex_stringmap_,
              ContainerEq(stringmap_v_.operator map<string, string>()));
  EXPECT_THAT(ex_strings_,
              ContainerEq(strings_v_.operator vector<string>()));
  EXPECT_THAT(
      ex_stringmaps_,
      ContainerEq(stringmaps_v_.operator vector<map<string, string>>()));
  EXPECT_THAT(ex_uint16s_,
              ContainerEq(uint16s_v_.operator vector<uint16_t>()));
}

TEST_F(DBusAdaptorTest, Signatures) {
  EXPECT_TRUE(DBusAdaptor::IsBool(bool_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsByteArrays(bytearrays_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsByte(byte_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsInt16(int16_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsInt32(int32_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsPath(path_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsPaths(paths_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsString(string_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsStringmap(stringmap_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsStrings(strings_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsUint16(uint16_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsUint16s(uint16s_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsUint32(uint32_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsUint64(uint64_v_.signature()));

  EXPECT_FALSE(DBusAdaptor::IsBool(byte_v_.signature()));
  EXPECT_FALSE(DBusAdaptor::IsStrings(string_v_.signature()));
}

template <typename T>
class DBusAdaptorTypedTest : public DBusAdaptorTest {
 protected:
  T* property() { return &property_; }

 private:
  T property_{};  // value-initialize primitives
};

TYPED_TEST_CASE(DBusAdaptorTypedTest, PropertyStoreTest::PropertyTypes);

TYPED_TEST(DBusAdaptorTypedTest, GetProperties) {
  MockPropertyStore store;
  const string kPropertyName("some property");
  PropertyStoreTest::RegisterProperty(&store, kPropertyName, this->property());

  map<string, ::DBus::Variant> properties;
  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::GetProperties(store, &properties, &error));
  EXPECT_TRUE(ContainsKey(properties, kPropertyName));
}

TEST_F(DBusAdaptorTest, SetProperty) {
  MockPropertyStore store;
  ::DBus::Error e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12;

  EXPECT_CALL(store, Contains(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(store, SetBoolProperty("", _, _)).WillOnce(Return(true));
  EXPECT_CALL(store, SetInt16Property("", _, _)).WillOnce(Return(true));
  EXPECT_CALL(store, SetInt32Property("", _, _)).WillOnce(Return(true));
  EXPECT_CALL(store, SetStringProperty("", _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetStringmapProperty("", _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetStringsProperty("", _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(store, SetUint8Property("", _, _)).WillOnce(Return(true));
  EXPECT_CALL(store, SetUint16Property("", _, _)).WillOnce(Return(true));
  EXPECT_CALL(store, SetUint16sProperty("", _, _)).WillOnce(Return(true));
  EXPECT_CALL(store, SetUint32Property("", _, _)).WillOnce(Return(true));
  EXPECT_CALL(store, SetUint64Property("", _, _)).WillOnce(Return(true));

  string string_path("/false/path");
  ::DBus::Path path(string_path);
  ::DBus::Variant path_v = DBusAdaptor::PathToVariant(path);
  EXPECT_CALL(store, SetStringProperty("", StrEq(string_path), _))
      .WillOnce(Return(true));

  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", bool_v_, &e1));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", path_v, &e2));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", string_v_, &e3));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", strings_v_, &e4));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", int16_v_, &e5));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", int32_v_, &e6));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", uint16_v_, &e7));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", uint16s_v_, &e8));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", uint32_v_, &e9));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", uint64_v_, &e10));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", stringmap_v_, &e11));
  EXPECT_TRUE(DBusAdaptor::SetProperty(&store, "", byte_v_, &e12));
}

// SetProperty method should propagate failures.  This should happen
// even if error isn't set. (This is to accomodate the fact that, if
// the property already has the desired value, the store will return
// false, without setting an error.)
TEST_F(DBusAdaptorTest, SetPropertyFailure) {
  MockPropertyStore store;
  ::DBus::Error e1, e2, e3, e4, e5, e6, e7, e8, e9, e10, e11, e12;

  EXPECT_CALL(store, Contains(_)).WillRepeatedly(Return(false));
  EXPECT_CALL(store, SetBoolProperty("", _, _)).WillOnce(Return(false));
  EXPECT_CALL(store, SetInt16Property("", _, _)).WillOnce(Return(false));
  EXPECT_CALL(store, SetInt32Property("", _, _)).WillOnce(Return(false));
  EXPECT_CALL(store, SetStringProperty("", _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(store, SetStringmapProperty("", _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(store, SetStringsProperty("", _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(store, SetUint8Property("", _, _)).WillOnce(Return(false));
  EXPECT_CALL(store, SetUint16Property("", _, _)).WillOnce(Return(false));
  EXPECT_CALL(store, SetUint16sProperty("", _, _)).WillOnce(Return(false));
  EXPECT_CALL(store, SetUint32Property("", _, _)).WillOnce(Return(false));
  EXPECT_CALL(store, SetUint64Property("", _, _)).WillOnce(Return(false));

  string string_path("/false/path");
  ::DBus::Path path(string_path);
  ::DBus::Variant path_v = DBusAdaptor::PathToVariant(path);
  EXPECT_CALL(store, SetStringProperty("", StrEq(string_path), _))
      .WillOnce(Return(false));

  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", bool_v_, &e1));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", path_v, &e2));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", string_v_, &e3));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", strings_v_, &e4));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", int16_v_, &e5));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", int32_v_, &e6));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", uint16_v_, &e7));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", uint16s_v_, &e8));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", uint32_v_, &e9));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", uint64_v_, &e10));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", stringmap_v_, &e11));
  EXPECT_FALSE(DBusAdaptor::SetProperty(&store, "", byte_v_, &e12));
}

void SetError(const string& /*name*/, Error* error) {
  error->Populate(Error::kInvalidProperty);
}

TEST_F(DBusAdaptorTest, ClearProperty) {
  MockPropertyStore store;
  ::DBus::Error e1, e2;

  EXPECT_CALL(store, ClearProperty("valid property", _)).
      WillOnce(Return(true));
  EXPECT_CALL(store, ClearProperty("invalid property", _)).
      WillOnce(DoAll(Invoke(SetError),
                     Return(false)));
  EXPECT_TRUE(DBusAdaptor::ClearProperty(&store, "valid property", &e1));
  EXPECT_FALSE(DBusAdaptor::ClearProperty(&store, "invalid property", &e2));
}

TEST_F(DBusAdaptorTest, ArgsToKeyValueStore) {
  map<string, ::DBus::Variant> args;
  KeyValueStore args_kv;
  Error error;

  const bool kBool = true;
  const char kBoolKey[] = "bool_arg";
  args[kBoolKey].writer().append_bool(kBool);
  const int32_t kInt32 = 123;
  const char kInt32Key[] = "int32_arg";
  args[kInt32Key].writer().append_int32(kInt32);
  const char kString[] = "string";
  const char kStringKey[] = "string_arg";
  args[kStringKey].writer().append_string(kString);
  const map<string, string> kStringmap{ { "key0", "value0" } };
  const char kStringmapKey[] = "stringmap_key";
  DBus::MessageIter writer = args[kStringmapKey].writer();
  writer << kStringmap;
  const vector<string> kStrings{ "string0", "string1" };
  const char kStringsKey[] = "strings_key";
  writer = args[kStringsKey].writer();
  writer << kStrings;
  map<string, ::DBus::Variant> variant_map;
  const char kVariantMapSubKey[] = "map_sub_key";
  variant_map[kVariantMapSubKey] = DBusAdaptor::BoolToVariant(true);
  const char kVariantMapKey[] = "map_key";
  writer = args[kVariantMapKey].writer();
  writer << variant_map;
  DBusAdaptor::ArgsToKeyValueStore(args, &args_kv, &error);
  EXPECT_TRUE(error.IsSuccess());
  EXPECT_EQ(kBool, args_kv.GetBool(kBoolKey));
  EXPECT_EQ(kInt32, args_kv.GetInt(kInt32Key));
  EXPECT_EQ(kString, args_kv.GetString(kStringKey));
  EXPECT_EQ(kStringmap, args_kv.GetStringmap(kStringmapKey));
  EXPECT_EQ(kStrings, args_kv.GetStrings(kStringsKey));
  KeyValueStore property_map;
  property_map.SetBool(kVariantMapSubKey, true);
  EXPECT_EQ(property_map, args_kv.GetKeyValueStore(kVariantMapKey));
}

TEST_F(DBusAdaptorTest, KeyValueStoreToVariant) {
  const char kStringmapKey[] = "StringmapKey";
  const map<string, string> kStringmapValue{ { "key", "value" } };
  const char kStringsKey[] = "StringsKey";
  const vector<string> kStringsValue{ "string0", "string1" };
  const char kStringKey[] = "StringKey";
  const char kStringValue[] = "StringValue";
  const char kBoolKey[] = "BoolKey";
  const bool kBoolValue = true;
  KeyValueStore store;
  store.SetStringmap(kStringmapKey, kStringmapValue);
  store.SetStrings(kStringsKey, kStringsValue);
  store.SetString(kStringKey, kStringValue);
  store.SetBool(kBoolKey, kBoolValue);
  DBus::Variant var = DBusAdaptor::KeyValueStoreToVariant(store);
  ASSERT_TRUE(DBusAdaptor::IsKeyValueStore(var.signature()));
  DBusPropertiesMap props = var.operator DBusPropertiesMap();
  // Sanity test the result.
  EXPECT_EQ(4, props.size());
  map<string, string> stringmap_value;
  EXPECT_TRUE(
      DBusProperties::GetStringmap(props, kStringmapKey, &stringmap_value));
  EXPECT_EQ(kStringmapValue, stringmap_value);
  vector<string> strings_value;
  EXPECT_TRUE(DBusProperties::GetStrings(props, kStringsKey, &strings_value));
  EXPECT_EQ(kStringsValue, strings_value);
  string string_value;
  EXPECT_TRUE(DBusProperties::GetString(props, kStringKey, &string_value));
  EXPECT_EQ(kStringValue, string_value);
  bool bool_value = !kBoolValue;
  EXPECT_TRUE(DBusProperties::GetBool(props, kBoolKey, &bool_value));
  EXPECT_EQ(kBoolValue, bool_value);
}

TEST_F(DBusAdaptorTest, SanitizePathElement) {
  EXPECT_EQ("0Ab_y_Z_9_", DBusAdaptor::SanitizePathElement("0Ab/y:Z`9{"));
  EXPECT_EQ("aB_f_0_Y_z", DBusAdaptor::SanitizePathElement("aB-f/0@Y[z"));
}

}  // namespace shill
