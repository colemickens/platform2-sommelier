// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mm/mm-modem.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "shill/cellular.h"
#include "shill/cellular_capability_gsm.h"
#include "shill/event_dispatcher.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_device_info.h"
#include "shill/mock_dbus_properties_proxy.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_metrics.h"
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
      : manager_(&control_interface_, &dispatcher_, &metrics_, &glib_),
        info_(&control_interface_, &dispatcher_, &metrics_, &manager_),
        proxy_(new MockDBusPropertiesProxy()),
        proxy_factory_(this),
        modem_(new Modem(kOwner,
                         kPath,
                         &control_interface_,
                         &dispatcher_,
                         &metrics_,
                         &manager_,
                         NULL)) {}

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
        DBusPropertiesProxyDelegate */*delegate*/,
        const string &/*path*/,
        const string &/*service*/) {
      return test_->proxy_.release();
    }

   private:
    ModemTest *test_;
  };

  static const char kOwner[];
  static const char kPath[];

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
  scoped_ptr<Modem> modem_;
  StrictMock<MockSockets> sockets_;
};

const char ModemTest::kOwner[] = ":1.18";
const char ModemTest::kPath[] = "/org/chromium/ModemManager/Gobi/0";

void ModemTest::SetUp() {
  EXPECT_EQ(kOwner, modem_->owner_);
  EXPECT_EQ(kPath, modem_->path_);
  modem_->proxy_factory_ = &proxy_factory_;
}

void ModemTest::TearDown() {
  modem_.reset();
  SetSockets(NULL);
}

TEST_F(ModemTest, Init) {
  DBusPropertiesMap props;

  SetSockets(&sockets_);
  props[Modem::kPropertyIPMethod].writer().append_uint32(
      MM_MODEM_IP_METHOD_DHCP);
  props[Modem::kPropertyLinkName].writer().append_string("usb1");
  EXPECT_CALL(*proxy_, GetAll(MM_MODEM_INTERFACE)).WillOnce(Return(props));
  modem_->Init();

  EXPECT_CALL(sockets_, Socket(PF_INET, SOCK_DGRAM, 0)).WillOnce(Return(-1));
  dispatcher_.DispatchPendingEvents();
}

TEST_F(ModemTest, CreateDeviceFromProperties) {
  DBusPropertiesMap props;

  modem_->CreateDeviceFromProperties(props);
  EXPECT_FALSE(modem_->device_.get());

  props[Modem::kPropertyIPMethod].writer().append_uint32(
      MM_MODEM_IP_METHOD_PPP);
  modem_->CreateDeviceFromProperties(props);
  EXPECT_FALSE(modem_->device_.get());

  props.erase(Modem::kPropertyIPMethod);
  props[Modem::kPropertyIPMethod].writer().append_uint32(
      MM_MODEM_IP_METHOD_DHCP);
  modem_->CreateDeviceFromProperties(props);
  EXPECT_FALSE(modem_->device_.get());

  static const char kLinkName[] = "usb0";
  static const unsigned char kAddress[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
  const int kTestSocket = 10;
  const int kTestLinkSocket = 11;
  props[Modem::kPropertyLinkName].writer().append_string(kLinkName);
  EXPECT_CALL(sockets_, Socket(PF_INET, SOCK_DGRAM, 0))
      .Times(2)
      .WillRepeatedly(Return(kTestSocket));
  EXPECT_CALL(sockets_, Ioctl(kTestSocket, SIOCGIFINDEX, _))
      .WillRepeatedly(DoAll(SetInterfaceIndex(), Return(0)));
  EXPECT_CALL(sockets_, Close(kTestSocket))
      .WillRepeatedly(Return(0));

  EXPECT_CALL(sockets_, Socket(PF_NETLINK, _, _))
      .WillRepeatedly(Return(kTestLinkSocket));
  EXPECT_CALL(sockets_, Bind(kTestLinkSocket, _, _))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(sockets_, Send(kTestLinkSocket, _, _, _))
      .WillRepeatedly(Return(0));
  RTNLHandler::GetInstance()->Start(&dispatcher_, &sockets_);

  ByteString expected_address(kAddress, arraysize(kAddress));
  EXPECT_CALL(info_, GetMACAddress(kTestInterfaceIndex, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(expected_address), Return(true)))
      .WillOnce(DoAll(SetArgumentPointee<1>(expected_address), Return(true)));
  EXPECT_CALL(info_, GetDevice(kTestInterfaceIndex))
      .WillRepeatedly(Return(modem_->device_));
  EXPECT_CALL(manager_, device_info()).WillRepeatedly(Return(&info_));

  modem_->CreateDeviceFromProperties(props);
  EXPECT_FALSE(modem_->device_.get());

  props[Modem::kPropertyType].writer().append_uint32(MM_MODEM_TYPE_GSM);
  props[Modem::kPropertyState].writer().append_uint32(
      Cellular::kModemStateDisabled);
  static const char kLockType[] = "sim-pin";
  const int kRetries = 2;
  props[CellularCapabilityGSM::kPropertyEnabledFacilityLocks].writer().
      append_uint32(MM_MODEM_GSM_FACILITY_SIM);
  props[CellularCapabilityGSM::kPropertyUnlockRequired].writer().append_string(
      kLockType);
  props[CellularCapabilityGSM::kPropertyUnlockRetries].writer().append_uint32(
      kRetries);
  modem_->CreateDeviceFromProperties(props);
  ASSERT_TRUE(modem_->device_.get());
  EXPECT_EQ(kLinkName, modem_->device_->link_name());
  EXPECT_EQ(kTestInterfaceIndex, modem_->device_->interface_index());
  EXPECT_EQ(Cellular::kModemStateDisabled, modem_->device_->modem_state());
  EXPECT_TRUE(GetCapabilityGSM()->sim_lock_status_.enabled);
  EXPECT_EQ(kLockType, GetCapabilityGSM()->sim_lock_status_.lock_type);
  EXPECT_EQ(kRetries, GetCapabilityGSM()->sim_lock_status_.retries_left);

  vector<DeviceRefPtr> devices;
  manager_.FilterByTechnology(Technology::kCellular, &devices);
  EXPECT_EQ(1, devices.size());
  EXPECT_TRUE(devices[0].get() == modem_->device_.get());
}

}  // namespace shill
