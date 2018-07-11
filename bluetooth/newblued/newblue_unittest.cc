// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/message_loop/message_loop.h>
#include <gtest/gtest.h>

#include "bluetooth/newblued/mock_libnewblue.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::SaveArg;

namespace bluetooth {

namespace {

constexpr uniq_t kDiscoveryHandle = 11;

}  // namespace

class NewblueTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto libnewblue = std::make_unique<MockLibNewblue>();
    libnewblue_ = libnewblue.get();
    newblue_ = std::make_unique<Newblue>(std::move(libnewblue));
  }

  bool StubHciUp(const uint8_t* address,
                 hciReadyForUpCbk callback,
                 void* callback_data) {
    callback(callback_data);
    return true;
  }

  void OnReadyForUp() { is_ready_for_up_ = true; }

  void OnDeviceDiscovered(const Device& device) {
    discovered_devices_.push_back(device);
  }

 protected:
  base::MessageLoop message_loop_;
  bool is_ready_for_up_ = false;
  std::unique_ptr<Newblue> newblue_;
  MockLibNewblue* libnewblue_;
  std::vector<Device> discovered_devices_;
};

TEST_F(NewblueTest, ListenReadyForUp) {
  newblue_->Init();

  EXPECT_CALL(*libnewblue_, HciUp(_, _, _))
      .WillOnce(Invoke(this, &NewblueTest::StubHciUp));
  bool success = newblue_->ListenReadyForUp(
      base::Bind(&NewblueTest::OnReadyForUp, base::Unretained(this)));
  EXPECT_TRUE(success);
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(is_ready_for_up_);
}

TEST_F(NewblueTest, ListenReadyForUpFailed) {
  newblue_->Init();

  EXPECT_CALL(*libnewblue_, HciUp(_, _, _)).WillOnce(Return(false));
  bool success = newblue_->ListenReadyForUp(
      base::Bind(&NewblueTest::OnReadyForUp, base::Unretained(this)));
  EXPECT_FALSE(success);
}

TEST_F(NewblueTest, BringUp) {
  EXPECT_CALL(*libnewblue_, HciIsUp()).WillOnce(Return(false));
  EXPECT_FALSE(newblue_->BringUp());

  EXPECT_CALL(*libnewblue_, HciIsUp()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, L2cInit()).WillOnce(Return(0));
  EXPECT_CALL(*libnewblue_, AttInit()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, GattProfileInit()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, GattBuiltinInit()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, SmInit(HCI_DISP_CAP_NONE)).WillOnce(Return(true));
  EXPECT_TRUE(newblue_->BringUp());
}

TEST_F(NewblueTest, StartDiscovery) {
  newblue_->Init();

  EXPECT_CALL(*libnewblue_, HciIsUp()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, L2cInit()).WillOnce(Return(0));
  EXPECT_CALL(*libnewblue_, AttInit()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, GattProfileInit()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, GattBuiltinInit()).WillOnce(Return(true));
  EXPECT_CALL(*libnewblue_, SmInit(HCI_DISP_CAP_NONE)).WillOnce(Return(true));
  EXPECT_TRUE(newblue_->BringUp());

  hciDeviceDiscoveredLeCbk inquiry_response_callback;
  void* inquiry_response_callback_data;
  EXPECT_CALL(*libnewblue_, HciDiscoverLeStart(_, _, true, false))
      .WillOnce(DoAll(SaveArg<0>(&inquiry_response_callback),
                      SaveArg<1>(&inquiry_response_callback_data),
                      Return(kDiscoveryHandle)));
  newblue_->StartDiscovery(
      base::Bind(&NewblueTest::OnDeviceDiscovered, base::Unretained(this)));

  // 2 devices discovered.
  struct bt_addr addr1 = {.type = BT_ADDR_TYPE_LE_RANDOM,
                          .addr = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06}};
  uint8_t eir1[] = {
      6, static_cast<uint8_t>(EirType::NAME_SHORT), 'a', 'l', 'i', 'c', 'e'};
  inquiry_response_callback(inquiry_response_callback_data, &addr1, -101,
                            HCI_ADV_TYPE_SCAN_RSP, &eir1, arraysize(eir1));
  struct bt_addr addr2 = {.type = BT_ADDR_TYPE_LE_PUBLIC,
                          .addr = {0x02, 0x03, 0x04, 0x05, 0x06, 0x07}};
  uint8_t eir2[] = {
      5, static_cast<uint8_t>(EirType::NAME_SHORT), 'b', 'o', 'b', '\0'};
  inquiry_response_callback(inquiry_response_callback_data, &addr2, -102,
                            HCI_ADV_TYPE_ADV_IND, &eir2, arraysize(eir2));
  // Scan response for device 1.
  uint8_t eir3[] = {4, static_cast<uint8_t>(EirType::CLASS_OF_DEV), 0x21, 0x22,
                    0x23};
  inquiry_response_callback(inquiry_response_callback_data, &addr1, -103,
                            HCI_ADV_TYPE_SCAN_RSP, &eir3, arraysize(eir3));

  message_loop_.RunUntilIdle();

  ASSERT_EQ(3, discovered_devices_.size());
  ASSERT_EQ("alice", discovered_devices_[0].name);
  ASSERT_EQ("06:05:04:03:02:01", discovered_devices_[0].address);
  ASSERT_EQ(-101, discovered_devices_[0].rssi);
  ASSERT_EQ("bob", discovered_devices_[1].name);
  ASSERT_EQ("07:06:05:04:03:02", discovered_devices_[1].address);
  ASSERT_EQ(-102, discovered_devices_[1].rssi);
  // The third discovery event should be an update to the first device, not a
  // new device.
  ASSERT_EQ("alice", discovered_devices_[2].name);
  ASSERT_EQ("06:05:04:03:02:01", discovered_devices_[2].address);
  ASSERT_EQ(-103, discovered_devices_[2].rssi);
  ASSERT_EQ(0x232221, discovered_devices_[2].eir_class);

  EXPECT_CALL(*libnewblue_, HciDiscoverLeStop(kDiscoveryHandle))
      .WillOnce(Return(true));
  newblue_->StopDiscovery();
  // Any inquiry response after StopDiscovery should be ignored.
  inquiry_response_callback(inquiry_response_callback_data, &addr1, -101,
                            HCI_ADV_TYPE_SCAN_RSP, &eir1, arraysize(eir1));
  message_loop_.RunUntilIdle();
  // Check that discovered_devices_ is still the same.
  ASSERT_EQ(3, discovered_devices_.size());
}

TEST_F(NewblueTest, UpdateEir) {
  Device device;
  uint8_t eir[] = {
      // Name
      4, static_cast<uint8_t>(EirType::NAME_SHORT), 'f', 'o', 'o',
      // Class
      4, static_cast<uint8_t>(EirType::CLASS_OF_DEV), 0x01, 0x02, 0x03,
      // Appearance
      3, static_cast<uint8_t>(EirType::GAP_APPEARANCE), 0x01, 0x02};
  Newblue::UpdateEir(&device, std::vector<uint8_t>(eir, eir + arraysize(eir)));
  ASSERT_EQ("foo", device.name);
  ASSERT_EQ(0x00030201, device.eir_class);
  ASSERT_EQ(0x0201, device.appearance);

  // Abnormal EIR data.
  uint8_t eir2[] = {
      // Containts non-ascii character
      5, static_cast<uint8_t>(EirType::NAME_SHORT), 0x80, 0x81, 'a', '\0',
      // Wrong field length (4, should be 3)
      4, static_cast<uint8_t>(EirType::GAP_APPEARANCE), 0x01, 0x02};
  Newblue::UpdateEir(&device,
                     std::vector<uint8_t>(eir2, eir2 + arraysize(eir2)));
  // Non-ascii characters are replaced with spaces.
  ASSERT_EQ("  a", device.name);
  // Class and Appearance should be unchanged.
  ASSERT_EQ(0x00030201, device.eir_class);
  ASSERT_EQ(0x0201, device.appearance);
}

}  // namespace bluetooth
