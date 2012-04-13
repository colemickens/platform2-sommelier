// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/modem.h"

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "shill/cellular.h"
#include "shill/cellular_capability_gsm.h"
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
#include "shill/mock_modem.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"

using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {

const int kTestInterfaceIndex = 5;
const char kLinkName[] = "usb0";
const char kOwner[] = ":1.18";
const char kPath[] = "/org/chromium/ModemManager/Gobi/0";
const unsigned char kAddress[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
const char kAddressAsString[] = "000102030405";

ACTION(SetInterfaceIndex) {
  if (arg2) {
    reinterpret_cast<struct ifreq *>(arg2)->ifr_ifindex = kTestInterfaceIndex;
  }
}

}  // namespace

class ModemTest : public Test {
 public:
  ModemTest()
      : manager_(&control_interface_, &dispatcher_, &metrics_, &glib_),
        info_(&control_interface_, &dispatcher_, &metrics_, &manager_),
        proxy_(new MockDBusPropertiesProxy()),
        proxy_factory_(this),
        modem_(
            new StrictModem(
                kOwner,
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
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(ModemTest *test) : test_(test) {}

    virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
        DBusPropertiesProxyDelegate */*delegate*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

   private:
    ModemTest *test_;
  };

  CellularCapabilityGSM *GetCapabilityGSM() {
    return dynamic_cast<CellularCapabilityGSM *>(
        modem_->device_->capability_.get());
  }

  MockGLib glib_;
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  MockMetrics metrics_;
  MockManager manager_;
  MockDeviceInfo info_;
  scoped_ptr<MockDBusPropertiesProxy> proxy_;
  TestProxyFactory proxy_factory_;
  scoped_ptr<StrictModem> modem_;
  MockRTNLHandler rtnl_handler_;
  ByteString expected_address_;
};

void ModemTest::SetUp() {
  EXPECT_EQ(kOwner, modem_->owner_);
  EXPECT_EQ(kPath, modem_->path_);
  ReplaceSingletons();
  expected_address_ = ByteString(kAddress, arraysize(kAddress));

  EXPECT_CALL(rtnl_handler_, GetInterfaceIndex(kLinkName)).
      WillRepeatedly(Return(kTestInterfaceIndex));

  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&info_));
}

void ModemTest::TearDown() {
  modem_.reset();
}

TEST_F(ModemTest, PendingDevicePropertiesAndCreate) {
  static const char kSentinel[] = "sentinel";
  static const uint32 kSentinelValue = 17;

  DBusPropertiesMap properties;
  properties[kSentinel].writer().append_uint32(kSentinelValue);

  EXPECT_CALL(*modem_, GetLinkName(_, _)).WillRepeatedly(DoAll(
      SetArgumentPointee<1>(string(kLinkName)),
      Return(true)));
  EXPECT_CALL(rtnl_handler_, GetInterfaceIndex(StrEq(kLinkName))).
      WillRepeatedly(Return(kTestInterfaceIndex));

  // The first time we call CreateDeviceFromModemProperties,
  // GetMACAddress will fail.
  EXPECT_CALL(info_, GetMACAddress(kTestInterfaceIndex, _)).
      WillOnce(Return(false));
  EXPECT_CALL(*modem_, GetModemInterface()).
      WillRepeatedly(Return(MM_MODEM_INTERFACE));
  modem_->CreateDeviceFromModemProperties(properties);
  EXPECT_FALSE(modem_->device_.get());

  // On the second time, we allow GetMACAddress to succeed.  Now we
  // expect a device to be built
  EXPECT_CALL(info_, GetMACAddress(kTestInterfaceIndex, _)).
      WillOnce(DoAll(SetArgumentPointee<1>(expected_address_),
                     Return(true)));

  // modem will take ownership
  MockCellular* cellular = new MockCellular(
      &control_interface_,
      &dispatcher_,
      &metrics_,
      &manager_,
      kLinkName,
      kAddressAsString,
      kTestInterfaceIndex,
      Cellular::kTypeGSM,
      kOwner,
      kPath,
      static_cast<mobile_provider_db *>(NULL));

  EXPECT_CALL(*modem_,
              ConstructCellular(StrEq(kLinkName),
                                StrEq(kAddressAsString),
                                kTestInterfaceIndex)).
      WillOnce(Return(cellular));

  EXPECT_CALL(*cellular, OnDBusPropertiesChanged(
      _,
      HasDBusPropertyWithValueU32(kSentinel, kSentinelValue),
      _));
  EXPECT_CALL(info_, RegisterDevice(_));
  EXPECT_CALL(info_, DeregisterDevice(_));
  modem_->OnDeviceInfoAvailable(kLinkName);

  EXPECT_TRUE(modem_->device_.get());
}

TEST_F(ModemTest, EarlyDeviceProperties) {
  // OnDeviceInfoAvailable called before
  // CreateDeviceFromModemProperties: Do nothing
  modem_->OnDeviceInfoAvailable(kLinkName);
  EXPECT_FALSE(modem_->device_.get());
}

TEST_F(ModemTest, CreateDeviceEarlyFailures) {
  DBusPropertiesMap properties;

  EXPECT_CALL(*modem_, ConstructCellular(_, _, _)).Times(0);

  // No link name:  no device created
  EXPECT_CALL(*modem_, GetLinkName(_, _)).WillOnce(Return(false));
  modem_->CreateDeviceFromModemProperties(properties);
  EXPECT_FALSE(modem_->device_.get());

  // Link name, but no ifindex: no device created
  EXPECT_CALL(*modem_, GetLinkName(_, _)).WillOnce(DoAll(
      SetArgumentPointee<1>(string(kLinkName)),
      Return(true)));
  EXPECT_CALL(rtnl_handler_, GetInterfaceIndex(StrEq(kLinkName))).WillOnce(
      Return(-1));
  modem_->CreateDeviceFromModemProperties(properties);
  EXPECT_FALSE(modem_->device_.get());
}

TEST_F(ModemTest, RejectPPPModem) {
  // TODO(rochberg):  Port this to ModemClassic
}

}  // namespace shill
