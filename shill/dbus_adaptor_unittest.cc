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
#include "shill/mock_service.h"
#include "shill/shill_event.h"

using std::map;
using std::string;
using std::vector;
using ::testing::Test;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

namespace shill {

class DBusAdaptorTest : public Test {
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
        manager_(&control_interface_, &dispatcher_),
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

    bool_v = DBusAdaptor::BoolToVariant(ex_bool);
    byte_v = DBusAdaptor::ByteToVariant(ex_byte);
    uint16_v = DBusAdaptor::Uint16ToVariant(ex_uint16);
    uint32_v = DBusAdaptor::Uint32ToVariant(ex_uint32);
    int16_v = DBusAdaptor::Int16ToVariant(ex_int16);
    int32_v = DBusAdaptor::Int32ToVariant(ex_int32);
    string_v = DBusAdaptor::StringToVariant(ex_string);
    stringmap_v = DBusAdaptor::StringmapToVariant(ex_stringmap);
    strings_v = DBusAdaptor::StringsToVariant(ex_strings);
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

  ::DBus::Variant bool_v;
  ::DBus::Variant byte_v;
  ::DBus::Variant uint16_v;
  ::DBus::Variant uint32_v;
  ::DBus::Variant int16_v;
  ::DBus::Variant int32_v;
  ::DBus::Variant string_v;
  ::DBus::Variant stringmap_v;
  ::DBus::Variant strings_v;

 protected:
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  Manager manager_;
  DeviceRefPtr device_;
  ServiceRefPtr service_;
};

TEST_F(DBusAdaptorTest, Conversions) {
  EXPECT_EQ(0, DBusAdaptor::BoolToVariant(0).reader().get_bool());
  EXPECT_EQ(ex_bool, bool_v.reader().get_bool());

  EXPECT_EQ(0, DBusAdaptor::ByteToVariant(0).reader().get_byte());
  EXPECT_EQ(ex_byte, byte_v.reader().get_byte());

  EXPECT_EQ(0, DBusAdaptor::Uint16ToVariant(0).reader().get_uint16());
  EXPECT_EQ(ex_uint16, uint16_v.reader().get_uint16());

  EXPECT_EQ(0, DBusAdaptor::Int16ToVariant(0).reader().get_int16());
  EXPECT_EQ(ex_int16, int16_v.reader().get_int16());

  EXPECT_EQ(0, DBusAdaptor::Uint32ToVariant(0).reader().get_uint32());
  EXPECT_EQ(ex_uint32, uint32_v.reader().get_uint32());

  EXPECT_EQ(0, DBusAdaptor::Int32ToVariant(0).reader().get_int32());
  EXPECT_EQ(ex_int32, int32_v.reader().get_int32());

  EXPECT_EQ(string(""), DBusAdaptor::StringToVariant("").reader().get_string());
  EXPECT_EQ(ex_string, string_v.reader().get_string());

  EXPECT_EQ(ex_stringmap[ex_string],
            (stringmap_v.operator map<string, string>()[ex_string]));
  EXPECT_EQ(ex_strings[0], strings_v.operator vector<string>()[0]);
}

TEST_F(DBusAdaptorTest, Signatures) {
  EXPECT_TRUE(DBusAdaptor::IsBool(bool_v.signature()));
  EXPECT_TRUE(DBusAdaptor::IsByte(byte_v.signature()));
  EXPECT_TRUE(DBusAdaptor::IsInt16(int16_v.signature()));
  EXPECT_TRUE(DBusAdaptor::IsInt32(int32_v.signature()));
  EXPECT_TRUE(DBusAdaptor::IsString(string_v.signature()));
  EXPECT_TRUE(DBusAdaptor::IsStringmap(stringmap_v.signature()));
  EXPECT_TRUE(DBusAdaptor::IsStrings(strings_v.signature()));
  EXPECT_TRUE(DBusAdaptor::IsUint16(uint16_v.signature()));
  EXPECT_TRUE(DBusAdaptor::IsUint32(uint32_v.signature()));

  EXPECT_FALSE(DBusAdaptor::IsBool(byte_v.signature()));
  EXPECT_FALSE(DBusAdaptor::IsStrings(string_v.signature()));
}

TEST_F(DBusAdaptorTest, Dispatch) {
  ::DBus::Error error;
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&manager_, "", bool_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(&manager_, "", string_v, error));

  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", strings_v, error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", int16_v, error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", int32_v, error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", uint16_v, error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", uint32_v, error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", stringmap_v, error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(&manager_, "", byte_v, error));

  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_.get(), "", bool_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_.get(), "", string_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_.get(), "", int16_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_.get(), "", int32_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_.get(), "", uint16_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(device_.get(), "", uint32_v, error));

  EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_.get(), "", byte_v, error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_.get(),
                                           "",
                                           strings_v,
                                           error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(device_.get(),
                                           "",
                                           stringmap_v,
                                           error));

  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_.get(), "", bool_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_.get(), "", byte_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_.get(), "", string_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_.get(), "", int32_v, error));
  EXPECT_TRUE(DBusAdaptor::DispatchOnType(service_.get(),
                                          "",
                                          stringmap_v,
                                          error));

  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service_.get(), "", int16_v, error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service_.get(),
                                           "",
                                           uint16_v,
                                           error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service_.get(),
                                           "",
                                           uint32_v,
                                           error));
  EXPECT_FALSE(DBusAdaptor::DispatchOnType(service_.get(),
                                           "",
                                           strings_v,
                                           error));
}

}  // namespace shill
