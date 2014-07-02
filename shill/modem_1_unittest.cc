// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem.h"

#include <base/files/scoped_temp_dir.h>
#include <ModemManager/ModemManager.h>

#include "shill/cellular_capability.h"
#include "shill/dbus_property_matchers.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_cellular.h"
#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_device_info.h"
#include "shill/mock_modem_info.h"
#include "shill/mock_proxy_factory.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/rtnl_handler.h"
#include "shill/testing.h"

using base::FilePath;
using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::Test;

namespace shill {

namespace {

const int kTestInterfaceIndex = 5;
const char kLinkName[] = "usb0";
const char kOwner[] = ":1.18";
const char kService[] = "org.chromium.ModemManager";
const char kPath[] = "/org/chromium/ModemManager/Gobi/0";
const unsigned char kAddress[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};

}  // namespace

class Modem1Test : public Test {
 public:
  Modem1Test()
      : modem_info_(NULL, &dispatcher_, NULL, NULL, NULL),
        device_info_(modem_info_.control_interface(), modem_info_.dispatcher(),
                     modem_info_.metrics(), modem_info_.manager()),
        proxy_(new MockDBusPropertiesProxy()),
        modem_(
            new Modem1(
                kOwner,
                kService,
                kPath,
                &modem_info_)) {}
  virtual void SetUp();
  virtual void TearDown();

  void ReplaceSingletons() {
    modem_->rtnl_handler_ = &rtnl_handler_;
    modem_->proxy_factory_ = &proxy_factory_;
  }

 protected:
  EventDispatcher dispatcher_;
  MockModemInfo modem_info_;
  MockDeviceInfo device_info_;
  scoped_ptr<MockDBusPropertiesProxy> proxy_;
  MockProxyFactory proxy_factory_;
  scoped_ptr<Modem1> modem_;
  MockRTNLHandler rtnl_handler_;
  ByteString expected_address_;
};

void Modem1Test::SetUp() {
  EXPECT_EQ(kOwner, modem_->owner_);
  EXPECT_EQ(kService, modem_->service_);
  EXPECT_EQ(kPath, modem_->path_);
  ReplaceSingletons();
  expected_address_ = ByteString(kAddress, arraysize(kAddress));

  EXPECT_CALL(rtnl_handler_, GetInterfaceIndex(kLinkName)).
      WillRepeatedly(Return(kTestInterfaceIndex));

  EXPECT_CALL(*modem_info_.mock_manager(), device_info())
      .WillRepeatedly(Return(&device_info_));
  EXPECT_CALL(device_info_, GetMACAddress(kTestInterfaceIndex, _)).
      WillOnce(DoAll(SetArgumentPointee<1>(expected_address_),
                     Return(true)));
}

void Modem1Test::TearDown() {
  modem_.reset();
}

TEST_F(Modem1Test, CreateDeviceMM1) {
  DBusInterfaceToProperties properties;
  DBusPropertiesMap modem_properties;
  DBus::Variant lock;
  lock.writer().append_uint32(MM_MODEM_LOCK_NONE);
  modem_properties[MM_MODEM_PROPERTY_UNLOCKREQUIRED] = lock;

  DBus::Variant ports_variant;
  DBus::MessageIter ports_message_iter = ports_variant.writer();
  DBus::MessageIter ports_array_iter = ports_message_iter.new_array("(su)");
  DBus::MessageIter port_struct_iter = ports_array_iter.new_struct();
  port_struct_iter.append_string(kLinkName);
  port_struct_iter.append_uint32(MM_MODEM_PORT_TYPE_NET);
  ports_array_iter.close_container(port_struct_iter);
  ports_message_iter.close_container(ports_array_iter);
  modem_properties[MM_MODEM_PROPERTY_PORTS] = ports_variant;

  properties[MM_DBUS_INTERFACE_MODEM] = modem_properties;

  DBusPropertiesMap modem3gpp_properties;
  DBus::Variant registration_state_variant;
  registration_state_variant.writer().append_uint32(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME);
  modem3gpp_properties[MM_MODEM_MODEM3GPP_PROPERTY_REGISTRATIONSTATE] =
      registration_state_variant;
  properties[MM_DBUS_INTERFACE_MODEM_MODEM3GPP] = modem3gpp_properties;

  EXPECT_CALL(proxy_factory_, CreateDBusPropertiesProxy(kPath, kOwner))
      .WillOnce(ReturnAndReleasePointee(&proxy_));
  modem_->CreateDeviceMM1(properties);
  EXPECT_TRUE(modem_->device().get());
  EXPECT_TRUE(modem_->device()->capability_->IsRegistered());
}

}  // namespace shill
