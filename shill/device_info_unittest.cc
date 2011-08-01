// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glib.h>
#include <sys/socket.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.
#include <linux/rtnetlink.h>

#include <base/callback_old.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop.h>
#include <base/stl_util-inl.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/device_info.h"
#include "shill/dhcp_provider.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"
#include "shill/mock_sockets.h"
#include "shill/rtnl_handler.h"

using std::string;
using testing::_;
using testing::Return;
using testing::Test;

namespace shill {

class TestEventDispatcher : public EventDispatcher {
 public:
  virtual IOInputHandler *CreateInputHandler(
      int fd,
      Callback1<InputData*>::Type *callback) {
    return NULL;
  }
};

class DeviceInfoTest : public Test {
 public:
  DeviceInfoTest()
      : manager_(&control_interface_, &dispatcher_, &glib_),
        device_info_(&control_interface_, &dispatcher_, &manager_),
        task_factory_(this) {
    DHCPProvider::GetInstance()->glib_ = &glib_;
  }

  base::hash_map<int, DeviceRefPtr> *DeviceInfoDevices() {
    return &device_info_.devices_;
  }

  void AddLink();

 protected:
  static const int kTestSocket;
  static const int kTestDeviceIndex;
  static const char kTestDeviceName[];

  void StartRTNLHandler();
  void StopRTNLHandler();

  MockGLib glib_;
  MockSockets sockets_;
  MockControl control_interface_;
  Manager manager_;
  DeviceInfo device_info_;
  TestEventDispatcher dispatcher_;
  ScopedRunnableMethodFactory<DeviceInfoTest> task_factory_;
};

const int DeviceInfoTest::kTestSocket = 123;
const int DeviceInfoTest::kTestDeviceIndex = 123456;
const char DeviceInfoTest::kTestDeviceName[] = "test-device";

void DeviceInfoTest::StartRTNLHandler() {
  EXPECT_CALL(sockets_, Socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE))
      .WillOnce(Return(kTestSocket));
  EXPECT_CALL(sockets_, Bind(kTestSocket, _, sizeof(sockaddr_nl)))
      .WillOnce(Return(0));
  RTNLHandler::GetInstance()->Start(&dispatcher_, &sockets_);
}

void DeviceInfoTest::StopRTNLHandler() {
  EXPECT_CALL(sockets_, Close(kTestSocket)).WillOnce(Return(0));
  RTNLHandler::GetInstance()->Stop();
}

void DeviceInfoTest::AddLink() {
  // TODO(petkov): Abstract this away into a separate message parsing/creation
  // class.
  struct {
    struct nlmsghdr hdr;
    struct ifinfomsg msg;
    char attrs[256];
  } message;
  memset(&message, 0, sizeof(message));
  message.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(message.msg));
  message.hdr.nlmsg_type = RTM_NEWLINK;
  message.msg.ifi_index = kTestDeviceIndex;

  // Append the interface name attribute.
  string link_name = kTestDeviceName;
  int len = RTA_LENGTH(link_name.size() + 1);
  size_t new_msg_size = NLMSG_ALIGN(message.hdr.nlmsg_len) + RTA_ALIGN(len);
  ASSERT_TRUE(new_msg_size <= sizeof(message));
  struct rtattr *rt_attr =
      reinterpret_cast<struct rtattr *>(reinterpret_cast<unsigned char *>
                                        (&message) +
                                        NLMSG_ALIGN(message.hdr.nlmsg_len));
  rt_attr->rta_type = IFLA_IFNAME;
  rt_attr->rta_len = len;
  memcpy(RTA_DATA(rt_attr), link_name.c_str(), link_name.size() + 1);
  message.hdr.nlmsg_len = new_msg_size;

  InputData data(reinterpret_cast<unsigned char *>(&message),
                 message.hdr.nlmsg_len);
  RTNLHandler::GetInstance()->ParseRTNL(&data);
}

TEST_F(DeviceInfoTest, DeviceEnumeration) {
  // Start our own private device_info
  device_info_.Start();
  EXPECT_CALL(sockets_, SendTo(kTestSocket, _, _, 0, _, sizeof(sockaddr_nl)))
      .WillOnce(Return(0));
  StartRTNLHandler();

  AddLink();

  // Peek in at the map of devices
  base::hash_map<int, DeviceRefPtr> *device_map = DeviceInfoDevices();
  EXPECT_EQ(1, device_map->size());
  EXPECT_TRUE(ContainsKey(*device_map, kTestDeviceIndex));

  device_info_.Stop();
  StopRTNLHandler();
}

TEST_F(DeviceInfoTest, DeviceEnumerationReverse) {
  // Start our own private device_info _after_ RTNLHandler has been started
  // TODO(pstew): Find out why this EXPECT_CALL needed to be moved above
  //              StartRTNLHandler()
  EXPECT_CALL(sockets_, SendTo(kTestSocket, _, _, 0, _, sizeof(sockaddr_nl)))
      .WillOnce(Return(0));
  StartRTNLHandler();
  device_info_.Start();

  AddLink();

  // Peek in at the map of devices
  base::hash_map<int, DeviceRefPtr> *device_map = DeviceInfoDevices();
  EXPECT_EQ(1, device_map->size());
  EXPECT_TRUE(ContainsKey(*device_map, kTestDeviceIndex));

  StopRTNLHandler();
  device_info_.Stop();
}

TEST_F(DeviceInfoTest, DeviceBlackList) {
  device_info_.AddDeviceToBlackList(kTestDeviceName);
  device_info_.Start();
  EXPECT_CALL(sockets_, SendTo(_, _, _, _, _, _));
  StartRTNLHandler();
  AddLink();

  // Peek in at the map of devices
  base::hash_map<int, DeviceRefPtr> *device_map(DeviceInfoDevices());
  ASSERT_TRUE(ContainsKey(*device_map, kTestDeviceIndex));
  EXPECT_TRUE(device_map->find(kTestDeviceIndex)->second->TechnologyIs(
      Device::kBlacklisted));

  StopRTNLHandler();
  device_info_.Stop();
}

}  // namespace shill
