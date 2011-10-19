// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "shill/cellular.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_device_info.h"
#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_sockets.h"
#include "shill/modem.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"

using std::string;
using std::vector;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::Test;

namespace shill {

namespace {

const int kTestInterfaceIndex = 5;

ACTION(SetInterfaceIndex) {
  if (arg2) {
    reinterpret_cast<struct ifreq *>(arg2)->ifr_ifindex = kTestInterfaceIndex;
  }
}

}  // namespace

class ModemTest : public Test {
 public:
  ModemTest()
      : manager_(&control_interface_, &dispatcher_, &glib_),
        proxy_(new MockDBusPropertiesProxy()),
        proxy_factory_(this),
        modem_(kOwner, kPath, &control_interface_, &dispatcher_, &manager_) {}

  virtual void SetUp();
  virtual void TearDown();

  void SetSockets(Sockets *sockets) {
    RTNLHandler::GetInstance()->sockets_ = sockets;
  }

 protected:
  class TestProxyFactory : public ProxyFactory {
   public:
    explicit TestProxyFactory(ModemTest *test) : test_(test) {}

    virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
        DBusPropertiesProxyListener */*listener*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

   private:
    ModemTest *test_;
  };

  static const char kOwner[];
  static const char kPath[];

  MockGLib glib_;
  MockControl control_interface_;
  EventDispatcher dispatcher_;
  MockManager manager_;
  scoped_ptr<MockDBusPropertiesProxy> proxy_;
  TestProxyFactory proxy_factory_;
  Modem modem_;
  StrictMock<MockSockets> sockets_;
};

const char ModemTest::kOwner[] = ":1.18";
const char ModemTest::kPath[] = "/org/chromium/ModemManager/Gobi/0";

void ModemTest::SetUp() {
  EXPECT_EQ(kOwner, modem_.owner_);
  EXPECT_EQ(kPath, modem_.path_);
  modem_.proxy_factory_ = &proxy_factory_;
  SetSockets(&sockets_);
}

void ModemTest::TearDown() {
  modem_.proxy_factory_ = NULL;
  SetSockets(NULL);
}

TEST_F(ModemTest, Init) {
  DBusPropertiesMap props;
  props[Modem::kPropertyIPMethod].writer().append_uint32(
      MM_MODEM_IP_METHOD_DHCP);
  props[Modem::kPropertyLinkName].writer().append_string("usb1");
  EXPECT_CALL(*proxy_, GetAll(MM_MODEM_INTERFACE)).WillOnce(Return(props));
  EXPECT_TRUE(modem_.task_factory_.empty());
  modem_.Init();
  EXPECT_FALSE(modem_.task_factory_.empty());

  EXPECT_CALL(sockets_, Socket(PF_INET, SOCK_DGRAM, 0)).WillOnce(Return(-1));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(ModemTest, CreateCellularDevice) {
  DBusPropertiesMap props;

  modem_.CreateCellularDevice(props);
  EXPECT_FALSE(modem_.device_.get());

  props[Modem::kPropertyIPMethod].writer().append_uint32(
      MM_MODEM_IP_METHOD_PPP);
  modem_.CreateCellularDevice(props);
  EXPECT_FALSE(modem_.device_.get());

  props.erase(Modem::kPropertyIPMethod);
  props[Modem::kPropertyIPMethod].writer().append_uint32(
      MM_MODEM_IP_METHOD_DHCP);
  modem_.CreateCellularDevice(props);
  EXPECT_FALSE(modem_.device_.get());

  static const char kLinkName[] = "usb0";
  static const unsigned char kAddress[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
  const int kTestSocket = 10;
  props[Modem::kPropertyLinkName].writer().append_string(kLinkName);
  EXPECT_CALL(sockets_, Socket(PF_INET, SOCK_DGRAM, 0))
      .Times(2)
      .WillRepeatedly(Return(kTestSocket));
  EXPECT_CALL(sockets_, Ioctl(kTestSocket, SIOCGIFINDEX, _))
      .WillRepeatedly(DoAll(SetInterfaceIndex(), Return(0)));
  EXPECT_CALL(sockets_, Close(kTestSocket))
      .WillRepeatedly(Return(0));

  ByteString expected_address(kAddress, arraysize(kAddress));
  MockDeviceInfo info_(&control_interface_, &dispatcher_, &manager_);
  EXPECT_CALL(info_, GetMACAddress(kTestInterfaceIndex, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(expected_address), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<1>(expected_address), Return(true)));
  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&info_));

  modem_.CreateCellularDevice(props);
  EXPECT_FALSE(modem_.device_.get());

  props[Modem::kPropertyType].writer().append_uint32(MM_MODEM_TYPE_CDMA);
  props[Modem::kPropertyState].writer().append_uint32(
      Cellular::kModemStateDisabled);
  modem_.CreateCellularDevice(props);
  ASSERT_TRUE(modem_.device_.get());
  EXPECT_EQ(kLinkName, modem_.device_->link_name());
  EXPECT_EQ(kTestInterfaceIndex, modem_.device_->interface_index());
  EXPECT_EQ(Cellular::kModemStateDisabled, modem_.device_->modem_state());

  vector<DeviceRefPtr> devices;
  manager_.FilterByTechnology(Technology::kCellular, &devices);
  EXPECT_EQ(1, devices.size());
  EXPECT_TRUE(devices[0].get() == modem_.device_.get());
}

}  // namespace shill
