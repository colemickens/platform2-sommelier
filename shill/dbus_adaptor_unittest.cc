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
using ::testing::Return;
using ::testing::StrEq;
using ::testing::Test;

namespace shill {

class DBusAdaptorTest : public PropertyStoreTest {
 public:
  DBusAdaptorTest()
      : ex_bool_(true),
        ex_byte_(0xff),
        ex_uint16_(65535),
        ex_uint32_(2000000),
        ex_int16_(-32768),
        ex_int32_(-65536),
        ex_path_("/"),
        ex_string_("something"),
        ex_strings_(1, ex_string_),
        bool_v_(DBusAdaptor::BoolToVariant(ex_bool_)),
        byte_v_(DBusAdaptor::ByteToVariant(ex_byte_)),
        int16_v_(DBusAdaptor::Int16ToVariant(ex_int16_)),
        int32_v_(DBusAdaptor::Int32ToVariant(ex_int32_)),
        path_v_(DBusAdaptor::PathToVariant(ex_path_)),
        string_v_(DBusAdaptor::StringToVariant(ex_string_)),
        strings_v_(DBusAdaptor::StringsToVariant(ex_strings_)),
        uint16_v_(DBusAdaptor::Uint16ToVariant(ex_uint16_)),
        uint32_v_(DBusAdaptor::Uint32ToVariant(ex_uint32_)),
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
                                 manager())) {
    ex_bytearrays_.push_back(ByteArray());
    bytearrays_v_ = DBusAdaptor::ByteArraysToVariant(ex_bytearrays_);

    ex_stringmap_[ex_string_] = ex_string_;
    stringmap_v_ = DBusAdaptor::StringmapToVariant(ex_stringmap_);

    ex_patharray_.push_back(ex_path_);
    patharray_v_ = DBusAdaptor::PathArrayToVariant(ex_patharray_);
  }

  virtual ~DBusAdaptorTest() {}

 protected:
  bool ex_bool_;
  uint8 ex_byte_;
  ByteArrays ex_bytearrays_;
  uint16 ex_uint16_;
  uint32 ex_uint32_;
  int16 ex_int16_;
  int32 ex_int32_;
  ::DBus::Path ex_path_;
  vector< ::DBus::Path> ex_patharray_;
  string ex_string_;
  map<string, string> ex_stringmap_;
  vector<map<string, string> > ex_stringmaps_;
  vector<string> ex_strings_;

  ::DBus::Variant bool_v_;
  ::DBus::Variant byte_v_;
  ::DBus::Variant bytearrays_v_;
  ::DBus::Variant int16_v_;
  ::DBus::Variant int32_v_;
  ::DBus::Variant path_v_;
  ::DBus::Variant patharray_v_;
  ::DBus::Variant string_v_;
  ::DBus::Variant stringmap_v_;
  ::DBus::Variant stringmaps_v_;
  ::DBus::Variant strings_v_;
  ::DBus::Variant uint16_v_;
  ::DBus::Variant uint32_v_;

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

  EXPECT_EQ(0, PropertyStoreTest::kInt32V.reader().get_int32());
  EXPECT_EQ(ex_int32_, int32_v_.reader().get_int32());

  EXPECT_EQ(ex_path_, path_v_.reader().get_path());

  EXPECT_EQ(ex_path_, patharray_v_.operator vector< ::DBus::Path>()[0]);

  EXPECT_EQ(string(""), PropertyStoreTest::kStringV.reader().get_string());
  EXPECT_EQ(ex_string_, string_v_.reader().get_string());

  EXPECT_EQ(ex_stringmap_[ex_string_],
            (stringmap_v_.operator map<string, string>()[ex_string_]));
  EXPECT_EQ(ex_strings_[0], strings_v_.operator vector<string>()[0]);
}

TEST_F(DBusAdaptorTest, Signatures) {
  EXPECT_TRUE(DBusAdaptor::IsBool(bool_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsByteArrays(bytearrays_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsByte(byte_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsInt16(int16_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsInt32(int32_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsPath(path_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsPathArray(patharray_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsString(string_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsStringmap(stringmap_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsStrings(strings_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsUint16(uint16_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsUint32(uint32_v_.signature()));

  EXPECT_FALSE(DBusAdaptor::IsBool(byte_v_.signature()));
  EXPECT_FALSE(DBusAdaptor::IsStrings(string_v_.signature()));
}

TEST_F(DBusAdaptorTest, Dispatch) {
  MockPropertyStore store;
  ::DBus::Error e1, e2, e3, e4, e5, e6, e7, e8, e9, e10;

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
  EXPECT_CALL(store, SetUint32Property("", _, _)).WillOnce(Return(true));

  string string_path("/false/path");
  ::DBus::Path path(string_path);
  ::DBus::Variant path_v = DBusAdaptor::PathToVariant(path);
  EXPECT_CALL(store, SetStringProperty("", StrEq(string_path), _))
      .WillOnce(Return(true));

  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", bool_v_, &e1));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", path_v, &e2));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", string_v_, &e3));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", strings_v_, &e4));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", int16_v_, &e5));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", int32_v_, &e6));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", uint16_v_, &e7));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", uint32_v_, &e8));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", stringmap_v_, &e9));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", byte_v_, &e10));
}

TEST_F(DBusAdaptorTest, ArgsToKeyValueStore) {
    map<string, ::DBus::Variant> args;
    KeyValueStore args_kv;
    Error error;

    args["string_arg"].writer().append_string("string");
    args["bool_arg"].writer().append_bool(true);
    DBusAdaptor::ArgsToKeyValueStore(args, &args_kv, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ("string", args_kv.GetString("string_arg"));
    EXPECT_EQ(true, args_kv.GetBool("bool_arg"));
}

}  // namespace shill
