// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include <mm/mm-modem.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "shill/cellular.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_glib.h"
#include "shill/mock_sockets.h"
#include "shill/modem.h"
#include "shill/proxy_factory.h"
#include "shill/rtnl_handler.h"
#include "shill/shill_event.h"

using std::string;
using testing::_;
using testing::DoAll;
using testing::Return;
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
    TestProxyFactory(ModemTest *test) : test_(test) {}

    virtual DBusPropertiesProxyInterface *CreateDBusPropertiesProxy(
        DBusPropertiesProxyListener *listener,
        const string &path,
        const string &service) {
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
  Manager manager_;
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
  ProxyFactory::set_factory(&proxy_factory_);
  SetSockets(&sockets_);
}

void ModemTest::TearDown() {
  ProxyFactory::set_factory(NULL);
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
  const int kTestSocket = 10;
  props[Modem::kPropertyLinkName].writer().append_string(kLinkName);
  EXPECT_CALL(sockets_, Socket(PF_INET, SOCK_DGRAM, 0))
      .Times(2)
      .WillRepeatedly(Return(kTestSocket));
  EXPECT_CALL(sockets_, Ioctl(kTestSocket, SIOCGIFINDEX, _))
      .WillRepeatedly(DoAll(SetInterfaceIndex(), Return(0)));
  EXPECT_CALL(sockets_, Close(kTestSocket))
      .WillRepeatedly(Return(0));
  modem_.CreateCellularDevice(props);
  EXPECT_FALSE(modem_.device_.get());

  props[Modem::kPropertyType].writer().append_uint32(MM_MODEM_TYPE_CDMA);
  modem_.CreateCellularDevice(props);
  ASSERT_TRUE(modem_.device_.get());
  EXPECT_EQ(kLinkName, modem_.device_->link_name());
  EXPECT_EQ(kTestInterfaceIndex, modem_.device_->interface_index());
  // TODO(petkov): Confirm the device is registered by the manager.
}

}  // namespace shill
