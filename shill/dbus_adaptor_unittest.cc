// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_device.h"
#include "shill/mock_property_store.h"
#include "shill/mock_service.h"
#include "shill/property_store_unittest.h"
#include "shill/shill_event.h"

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
      : ex_bool(true),
        ex_byte(0xff),
        ex_uint16(65535),
        ex_uint32(2000000),
        ex_int16(-32768),
        ex_int32(-65536),
        ex_string("something"),
        ex_strings(1, ex_string),
        device_(new MockDevice(&control_interface_,
                               &dispatcher_,
                               &manager_,
                               "mock",
                               0)),
        service_(new MockService(&control_interface_,
                                 &dispatcher_,
                                 device_,
                                 "mock")) {
    ex_stringmap[ex_string] = ex_string;

    bool_v_ = DBusAdaptor::BoolToVariant(ex_bool);
    byte_v_ = DBusAdaptor::ByteToVariant(ex_byte);
    uint16_v_ = DBusAdaptor::Uint16ToVariant(ex_uint16);
    uint32_v_ = DBusAdaptor::Uint32ToVariant(ex_uint32);
    int16_v_ = DBusAdaptor::Int16ToVariant(ex_int16);
    int32_v_ = DBusAdaptor::Int32ToVariant(ex_int32);
    string_v_ = DBusAdaptor::StringToVariant(ex_string);
    stringmap_v_ = DBusAdaptor::StringmapToVariant(ex_stringmap);
    strings_v_ = DBusAdaptor::StringsToVariant(ex_strings);
  }

  virtual ~DBusAdaptorTest() {}

  bool ex_bool;
  uint8 ex_byte;
  uint16 ex_uint16;
  uint32 ex_uint32;
  int16 ex_int16;
  int32 ex_int32;
  string ex_string;
  map<string, string> ex_stringmap;
  vector<string> ex_strings;

 protected:
  DeviceRefPtr device_;
  ServiceRefPtr service_;
};

TEST_F(DBusAdaptorTest, Conversions) {
  EXPECT_EQ(0, DBusAdaptor::BoolToVariant(0).reader().get_bool());
  EXPECT_EQ(ex_bool, bool_v_.reader().get_bool());

  EXPECT_EQ(0, DBusAdaptor::ByteToVariant(0).reader().get_byte());
  EXPECT_EQ(ex_byte, byte_v_.reader().get_byte());

  EXPECT_EQ(0, DBusAdaptor::Uint16ToVariant(0).reader().get_uint16());
  EXPECT_EQ(ex_uint16, uint16_v_.reader().get_uint16());

  EXPECT_EQ(0, DBusAdaptor::Int16ToVariant(0).reader().get_int16());
  EXPECT_EQ(ex_int16, int16_v_.reader().get_int16());

  EXPECT_EQ(0, DBusAdaptor::Uint32ToVariant(0).reader().get_uint32());
  EXPECT_EQ(ex_uint32, uint32_v_.reader().get_uint32());

  EXPECT_EQ(0, DBusAdaptor::Int32ToVariant(0).reader().get_int32());
  EXPECT_EQ(ex_int32, int32_v_.reader().get_int32());

  EXPECT_EQ(string(""), DBusAdaptor::StringToVariant("").reader().get_string());
  EXPECT_EQ(ex_string, string_v_.reader().get_string());

  EXPECT_EQ(ex_stringmap[ex_string],
            (stringmap_v_.operator map<string, string>()[ex_string]));
  EXPECT_EQ(ex_strings[0], strings_v_.operator vector<string>()[0]);
}

TEST_F(DBusAdaptorTest, Signatures) {
  EXPECT_TRUE(DBusAdaptor::IsBool(bool_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsByte(byte_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsInt16(int16_v_.signature()));
  EXPECT_TRUE(DBusAdaptor::IsInt32(int32_v_.signature()));
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
  ::DBus::Error error;

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

  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", bool_v_, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", path_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", string_v_, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", strings_v_, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", int16_v_, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", int32_v_, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", uint16_v_, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", uint32_v_, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", stringmap_v_, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&store, "", byte_v_, error));

}

}  // namespace shill
