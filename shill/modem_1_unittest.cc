// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem.h"

#include <base/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ModemManager/ModemManager-enums.h>
#include <ModemManager/ModemManager-names.h>

#include "shill/cellular_capability.h"
#include "shill/dbus_property_matchers.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_cellular.h"
#include "shill/mock_control.h"
#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_device_info.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"

using std::string;
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
const char kAddressAsString[] = "000102030405";

}  // namespace

class Modem1Test : public Test {
 public:
  Modem1Test()
      : manager_(&control_interface_, &dispatcher_, &metrics_, &glib_),
        info_(&control_interface_, &dispatcher_, &metrics_, &manager_),
        proxy_(new MockDBusPropertiesProxy()),
        proxy_factory_(this),
        modem_(
            new Modem1(
                kOwner,
                kService,
                kPath,
                &control_interface_,
                &dispatcher_,
                &metrics_,
                &manager_,
                static_cast<mobile_provider_db *>(NULL))) {}
  virtual void SetUp();
  virtual void TearDown();

  void ReplaceSingletons() {
    modem_->rtnl_handler_ = &rtnl_handler_;
    modem_->proxy_factory_ = &proxy_factory_;
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(Modem1Test *test) : test_(test) {}

    virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

   private:
    Modem1Test *test_;
  };

  MockGLib glib_;
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDeviceInfo info_;
  scoped_ptr<MockDBusPropertiesProxy> proxy_;
  TestProxyFactory proxy_factory_;
  scoped_ptr<Modem1> modem_;
  MockRTNLHandler rtnl_handler_;
  ByteString expected_address_;
  ScopedTempDir temp_dir_;
  string device_;
};

void Modem1Test::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  modem_->netfiles_path_ = temp_dir_.path();
  device_ = temp_dir_.path().Append("devices").Append(kLinkName).value();
  FilePath device_dir = FilePath(device_).Append("1-2/3-4");
  ASSERT_TRUE(file_util::CreateDirectory(device_dir));
  FilePath symlink(temp_dir_.path().Append(kLinkName));
  ASSERT_TRUE(file_util::CreateSymbolicLink(device_dir, symlink));

  EXPECT_EQ(kOwner, modem_->owner_);
  EXPECT_EQ(kService, modem_->service_);
  EXPECT_EQ(kPath, modem_->path_);
  ReplaceSingletons();
  expected_address_ = ByteString(kAddress, arraysize(kAddress));

  EXPECT_CALL(rtnl_handler_, GetInterfaceIndex(kLinkName)).
      WillRepeatedly(Return(kTestInterfaceIndex));

  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&info_));
  EXPECT_CALL(info_, GetMACAddress(kTestInterfaceIndex, _)).
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
  DBus::Variant device_variant;
  device_variant.writer().append_string(device_.c_str());
  modem_properties[MM_MODEM_PROPERTY_DEVICE] = device_variant;
  properties[MM_DBUS_INTERFACE_MODEM] = modem_properties;

  DBusPropertiesMap modem3gpp_properties;
  DBus::Variant registration_state_variant;
  registration_state_variant.writer().append_uint32(
      MM_MODEM_3GPP_REGISTRATION_STATE_HOME);
  modem3gpp_properties[MM_MODEM_MODEM3GPP_PROPERTY_REGISTRATIONSTATE] =
      registration_state_variant;
  properties[MM_DBUS_INTERFACE_MODEM_MODEM3GPP] = modem3gpp_properties;

  modem_->CreateDeviceMM1(properties);
  EXPECT_TRUE(modem_->device().get());
  EXPECT_TRUE(modem_->device()->capability_->IsRegistered());
}

}  // namespace shill
