// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_info.h"

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

#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"
#include "shill/mock_sockets.h"
#include "shill/rtnl_handler.h"
#include "shill/rtnl_message.h"

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
        device_info_(&control_interface_, &dispatcher_, &manager_) {
  }

  base::hash_map<int, DeviceRefPtr> *DeviceInfoDevices() {
    return &device_info_.devices_;
  }

 protected:
  static const int kTestDeviceIndex;
  static const char kTestDeviceName[];

  RTNLMessage *BuildMessage(RTNLMessage::MessageType type,
                            RTNLMessage::MessageMode mode);
  void SendMessageToDeviceInfo(const RTNLMessage &message);

  MockGLib glib_;
  MockControl control_interface_;
  Manager manager_;
  DeviceInfo device_info_;
  TestEventDispatcher dispatcher_;
};

const int DeviceInfoTest::kTestDeviceIndex = 123456;
const char DeviceInfoTest::kTestDeviceName[] = "test-device";

RTNLMessage *DeviceInfoTest::BuildMessage(RTNLMessage::MessageType type,
                                          RTNLMessage::MessageMode mode) {
  RTNLMessage *message = new RTNLMessage(type, mode, 0, 0, 0, kTestDeviceIndex,
                                         IPAddress::kAddressFamilyIPv4);
  message->SetAttribute(static_cast<uint16>(IFLA_IFNAME),
                        ByteString(kTestDeviceName, true));
  return message;
}

void DeviceInfoTest::SendMessageToDeviceInfo(const RTNLMessage &message) {
  device_info_.LinkMsgHandler(message);
}

TEST_F(DeviceInfoTest, DeviceEnumeration) {
  // Start our own private device_info
  device_info_.Start();
  scoped_ptr<RTNLMessage> message(BuildMessage(RTNLMessage::kMessageTypeLink,
                                               RTNLMessage::kMessageModeAdd));
  SendMessageToDeviceInfo(*message.get());

  // Peek in at the map of devices
  base::hash_map<int, DeviceRefPtr> *device_map = DeviceInfoDevices();
  EXPECT_EQ(1, device_map->size());
  EXPECT_TRUE(ContainsKey(*device_map, kTestDeviceIndex));

  message.reset(BuildMessage(RTNLMessage::kMessageTypeLink,
                             RTNLMessage::kMessageModeDelete));
  SendMessageToDeviceInfo(*message.get());

  // Peek in at the map of devices
  EXPECT_EQ(0, device_map->size());
  EXPECT_FALSE(ContainsKey(*device_map, kTestDeviceIndex));

  device_info_.Stop();
}

TEST_F(DeviceInfoTest, DeviceBlackList) {
  device_info_.AddDeviceToBlackList(kTestDeviceName);
  device_info_.Start();
  scoped_ptr<RTNLMessage> message(BuildMessage(RTNLMessage::kMessageTypeLink,
                                               RTNLMessage::kMessageModeAdd));
  SendMessageToDeviceInfo(*message.get());

  // Peek in at the map of devices
  base::hash_map<int, DeviceRefPtr> *device_map(DeviceInfoDevices());
  ASSERT_TRUE(ContainsKey(*device_map, kTestDeviceIndex));
  EXPECT_TRUE(device_map->find(kTestDeviceIndex)->second->TechnologyIs(
      Device::kBlacklisted));

  device_info_.Stop();
}

}  // namespace shill
