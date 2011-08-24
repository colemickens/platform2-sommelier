// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_info.h"

#include <glib.h>
#include <sys/socket.h>
#include <linux/if.h>
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

using std::map;
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

 protected:
  static const int kTestDeviceIndex;
  static const char kTestDeviceName[];
  static const char kTestAddress[];

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
const char DeviceInfoTest::kTestAddress[] = {
  0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };

RTNLMessage *DeviceInfoTest::BuildMessage(RTNLMessage::MessageType type,
                                          RTNLMessage::MessageMode mode) {
  RTNLMessage *message = new RTNLMessage(type, mode, 0, 0, 0, kTestDeviceIndex,
                                         IPAddress::kAddressFamilyIPv4);
  message->SetAttribute(static_cast<uint16>(IFLA_IFNAME),
                        ByteString(kTestDeviceName, true));
  ByteString test_address(kTestAddress, sizeof(kTestAddress));
  message->SetAttribute(IFLA_ADDRESS, test_address);
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
  message->set_link_status(RTNLMessage::LinkStatus(0, IFF_LOWER_UP, 0));
  EXPECT_FALSE(device_info_.GetDevice(kTestDeviceIndex).get());
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetDevice(kTestDeviceIndex).get());
  unsigned int flags = 0;
  EXPECT_TRUE(device_info_.GetFlags(kTestDeviceIndex, &flags));
  EXPECT_EQ(IFF_LOWER_UP, flags);
  ByteString address;
  EXPECT_TRUE(device_info_.GetAddress(kTestDeviceIndex, &address));
  EXPECT_FALSE(address.IsEmpty());
  EXPECT_TRUE(address.Equals(ByteString(kTestAddress, sizeof(kTestAddress))));

  message.reset(BuildMessage(RTNLMessage::kMessageTypeLink,
                             RTNLMessage::kMessageModeAdd));
  message->set_link_status(RTNLMessage::LinkStatus(0, IFF_UP | IFF_RUNNING, 0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetFlags(kTestDeviceIndex, &flags));
  EXPECT_EQ(IFF_UP | IFF_RUNNING, flags);

  message.reset(BuildMessage(RTNLMessage::kMessageTypeLink,
                             RTNLMessage::kMessageModeDelete));
  SendMessageToDeviceInfo(*message);
  EXPECT_FALSE(device_info_.GetDevice(kTestDeviceIndex).get());
  EXPECT_FALSE(device_info_.GetFlags(kTestDeviceIndex, NULL));

  device_info_.Stop();
}

TEST_F(DeviceInfoTest, DeviceBlackList) {
  device_info_.AddDeviceToBlackList(kTestDeviceName);
  device_info_.Start();
  scoped_ptr<RTNLMessage> message(BuildMessage(RTNLMessage::kMessageTypeLink,
                                               RTNLMessage::kMessageModeAdd));
  SendMessageToDeviceInfo(*message);

  DeviceRefPtr device = device_info_.GetDevice(kTestDeviceIndex);
  ASSERT_TRUE(device.get());
  EXPECT_TRUE(device->TechnologyIs(Device::kBlacklisted));

  device_info_.Stop();
}

}  // namespace shill
