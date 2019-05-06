// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/cec-funcs.h>

#include <utility>

#include <base/bind.h>
#include <base/callback.h>
#include <base/macros.h>
#include <base/files/file_path.h>
#include <gmock/gmock.h>

#include "cecservice/cec_device.h"
#include "cecservice/cec_fd_mock.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SaveArgPointee;
using ::testing::StrEq;

namespace cecservice {

namespace {
constexpr uint16_t kPhysicalAddress = 2;
constexpr uint8_t kLogicalAddress = CEC_LOG_ADDR_PLAYBACK_1;
constexpr uint8_t kOtherLogicalAddress = CEC_LOG_ADDR_PLAYBACK_3;
constexpr uint16_t kLogicalAddressMask = (1 << kLogicalAddress);

void Copy(TvPowerStatus* out, TvPowerStatus in) {
  *out = in;
}
}  // namespace

class CecDeviceTest : public ::testing::Test {
 public:
  CecDeviceTest();
  ~CecDeviceTest() = default;

 protected:
  // Performs initialization of CecDeviceImpl object.
  void Init();
  // Sets up physical and logical address on the device.
  void Connect();
  // Peforms the last stage of device initialization.
  void ConfigureTVAddress(uint8_t address);
  // Does the 2 above things at once.
  void ConnectAndConfigureTVAddress(uint8_t address);
  // Sets device into active source mode (by issuing ImageViewOn request).
  void SetActiveSource();
  // Sends state update event to the device.
  void SendStateUpdateEvent(uint16_t physical_address,
                            uint16_t logical_address_mask);

  // Sends state update event to the device.
  void SendAndCheckMessage(uint16_t source_addr,
                           uint16_t dest_addr,
                           uint8_t opcode);

  // Reads in a message initialized by provided function.
  void ReadMessageIn(struct cec_msg msg);

  CecFd::Callback event_callback_;
  CecFdMock* cec_fd_mock_;  // owned by |device_|
  std::unique_ptr<CecDeviceImpl> device_;
  struct cec_msg sent_message_ = {};

 private:
  DISALLOW_COPY_AND_ASSIGN(CecDeviceTest);
};

CecDeviceTest::CecDeviceTest() {
  auto cec_fd_mock = std::make_unique<NiceMock<CecFdMock>>();

  cec_fd_mock_ = cec_fd_mock.get();
  device_ = std::make_unique<CecDeviceImpl>(std::move(cec_fd_mock),
                                            base::FilePath("/fake_path"));

  ON_CALL(*cec_fd_mock_, TransmitMessage(_))
      .WillByDefault(DoAll(SaveArgPointee<0>(&sent_message_),
                           Return(CecFd::TransmitResult::kOk)));

  ON_CALL(*cec_fd_mock_, WriteWatch()).WillByDefault(Return(true));
}

void CecDeviceTest::Init() {
  ON_CALL(*cec_fd_mock_, SetEventCallback(_))
      .WillByDefault(DoAll(SaveArg<0>(&event_callback_), Return(true)));

  ON_CALL(*cec_fd_mock_, GetLogicalAddresses(_))
      .WillByDefault(Invoke([](struct cec_log_addrs* address) {
        address->num_log_addrs = 1;
        return true;
      }));

  device_->Init();
  ASSERT_FALSE(event_callback_.is_null());
}

void CecDeviceTest::ConfigureTVAddress(uint8_t address) {
  EXPECT_CALL(*cec_fd_mock_, ReceiveMessage(_))
      .WillOnce(Invoke([address](struct cec_msg* msg) {
        cec_msg_init(msg, address, CEC_LOG_ADDR_BROADCAST);
        cec_msg_report_physical_addr(msg, 0, CEC_OP_PRIM_DEVTYPE_TV);
        return true;
      }));
  // Read the active source request.
  event_callback_.Run(CecFd::EventType::kRead);
}

void CecDeviceTest::Connect() {
  SendStateUpdateEvent(kPhysicalAddress, kLogicalAddressMask);
}

void CecDeviceTest::ConnectAndConfigureTVAddress(uint8_t address) {
  Connect();
  ConfigureTVAddress(address);
}

void CecDeviceTest::SendStateUpdateEvent(uint16_t physical_address,
                                         uint16_t logical_address_mask) {
  ON_CALL(*cec_fd_mock_, ReceiveEvent(_))
      .WillByDefault(Invoke([=](struct cec_event* event) {
        event->event = CEC_EVENT_STATE_CHANGE;
        event->state_change.phys_addr = physical_address;
        event->state_change.log_addr_mask = logical_address_mask;
        event->flags = 0;

        return true;
      }));

  event_callback_.Run(CecFd::EventType::kPriorityRead);
}

void CecDeviceTest::SendAndCheckMessage(uint16_t source_addr,
                                        uint16_t dest_addr,
                                        uint8_t opcode) {
  sent_message_ = {};
  EXPECT_CALL(*cec_fd_mock_, TransmitMessage(_))
      .WillOnce(DoAll(SaveArgPointee<0>(&sent_message_),
                      Return(CecFd::TransmitResult::kOk)))
      .RetiresOnSaturation();
  event_callback_.Run(CecFd::EventType::kWrite);
  EXPECT_EQ(source_addr, cec_msg_initiator(&sent_message_));
  EXPECT_EQ(dest_addr, cec_msg_destination(&sent_message_));
  EXPECT_EQ(opcode, cec_msg_opcode(&sent_message_));
}

void CecDeviceTest::ReadMessageIn(struct cec_msg msg) {
  EXPECT_CALL(*cec_fd_mock_, ReceiveMessage(_))
      .WillOnce(Invoke([&](struct cec_msg* message) {
        *message = msg;
        return true;
      }));
  event_callback_.Run(CecFd::EventType::kRead);
}

void CecDeviceTest::SetActiveSource() {
  // To set the object as active source we will request wake up and let it
  // write image view on and active source messages (hence the 2 writes).
  device_->SetWakeUp();
  event_callback_.Run(CecFd::EventType::kWrite);
  event_callback_.Run(CecFd::EventType::kWrite);
}

TEST_F(CecDeviceTest, TestInitFail) {
  EXPECT_CALL(*cec_fd_mock_, SetEventCallback(_)).WillOnce(Return(false));
  EXPECT_CALL(*cec_fd_mock_, CecFdDestructorCalled());
  EXPECT_FALSE(device_->Init());
  // Verify that the fd has been destroyed at this point, i.e.
  // object has entered disabled state.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(cec_fd_mock_));
}

TEST_F(CecDeviceTest, TestLogicalAddressGetFail) {
  EXPECT_CALL(*cec_fd_mock_, SetEventCallback(_)).WillOnce(Return(true));
  EXPECT_CALL(*cec_fd_mock_, GetLogicalAddresses(_)).WillOnce(Return(false));
  EXPECT_CALL(*cec_fd_mock_, SetLogicalAddresses(_)).Times(0);
  EXPECT_CALL(*cec_fd_mock_, CecFdDestructorCalled());
  EXPECT_FALSE(device_->Init());
  // Verify that the fd has been destroyed at this point, i.e.
  // object has entered disabled state.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(cec_fd_mock_));
}

TEST_F(CecDeviceTest, TestLogicalAddressSetFail) {
  EXPECT_CALL(*cec_fd_mock_, SetEventCallback(_)).WillOnce(Return(true));
  EXPECT_CALL(*cec_fd_mock_, GetLogicalAddresses(_))
      .WillOnce(Invoke([](struct cec_log_addrs* address) {
        address->num_log_addrs = 0;
        return true;
      }));
  EXPECT_CALL(*cec_fd_mock_, SetLogicalAddresses(_)).WillOnce(Return(false));
  EXPECT_CALL(*cec_fd_mock_, CecFdDestructorCalled());
  EXPECT_FALSE(device_->Init());
  // Verify that the fd has been destroyed at this point, i.e.
  // object has entered disabled state.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(cec_fd_mock_));
}

// Test the basic logical address configuration flow.
TEST_F(CecDeviceTest, TestConnect) {
  EXPECT_CALL(*cec_fd_mock_, GetLogicalAddresses(_))
      .WillOnce(Invoke([](struct cec_log_addrs* address) {
        address->num_log_addrs = 0;
        return true;
      }));

  EXPECT_CALL(
      *cec_fd_mock_,
      SetLogicalAddresses(AllOf(
          Field(&cec_log_addrs::cec_version, CEC_OP_CEC_VERSION_1_4),
          Field(&cec_log_addrs::num_log_addrs, 1),
          Field(&cec_log_addrs::log_addr_type,
                ElementsAre(uint8_t(CEC_LOG_ADDR_TYPE_PLAYBACK), _, _, _)),
          Field(&cec_log_addrs::osd_name, StrEq("Chrome OS")),
          Field(&cec_log_addrs::flags, CEC_LOG_ADDRS_FL_ALLOW_UNREG_FALLBACK))))
      .WillOnce(Return(true));

  Init();

  SendStateUpdateEvent(kPhysicalAddress, 0);
  SendStateUpdateEvent(kPhysicalAddress, kLogicalAddressMask);

  ConfigureTVAddress(CEC_LOG_ADDR_TV);

  // Test if we are truly connected - if we are standby request should result in
  // write watch being requested.
  EXPECT_CALL(*cec_fd_mock_, WriteWatch());
  device_->SetStandBy();
}

TEST_F(CecDeviceTest, TestSendWakeUp) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  EXPECT_CALL(*cec_fd_mock_, WriteWatch())
      .Times(2)
      .WillRepeatedly(Return(true));
  device_->SetWakeUp();

  event_callback_.Run(CecFd::EventType::kWrite);

  EXPECT_EQ(kLogicalAddress, cec_msg_initiator(&sent_message_));
  EXPECT_EQ(CEC_LOG_ADDR_TV, cec_msg_destination(&sent_message_));
  EXPECT_EQ(CEC_MSG_IMAGE_VIEW_ON, cec_msg_opcode(&sent_message_));

  event_callback_.Run(CecFd::EventType::kWrite);
  EXPECT_EQ(kLogicalAddress, cec_msg_initiator(&sent_message_));
  EXPECT_EQ(CEC_LOG_ADDR_BROADCAST, cec_msg_destination(&sent_message_));
  EXPECT_EQ(CEC_MSG_ACTIVE_SOURCE, cec_msg_opcode(&sent_message_));
}

TEST_F(CecDeviceTest, TestSendWakeUpWhileDisconnected) {
  Init();

  device_->SetWakeUp();

  EXPECT_EQ(CEC_LOG_ADDR_UNREGISTERED, cec_msg_initiator(&sent_message_));
  EXPECT_EQ(CEC_LOG_ADDR_TV, cec_msg_destination(&sent_message_));
  EXPECT_EQ(CEC_MSG_IMAGE_VIEW_ON, cec_msg_opcode(&sent_message_));

  // Test that we hold off with requesting write until we have addresses
  // configured.
  EXPECT_CALL(*cec_fd_mock_, WriteWatch()).Times(0);
  event_callback_.Run(CecFd::EventType::kWrite);

  // We should start request write watching again while we connect.
  EXPECT_CALL(*cec_fd_mock_, WriteWatch());
  Connect();

  event_callback_.Run(CecFd::EventType::kWrite);
  EXPECT_EQ(kLogicalAddress, cec_msg_initiator(&sent_message_));
  EXPECT_EQ(CEC_LOG_ADDR_BROADCAST, cec_msg_destination(&sent_message_));
  EXPECT_EQ(CEC_MSG_ACTIVE_SOURCE, cec_msg_opcode(&sent_message_));
}

TEST_F(CecDeviceTest, TestActiveSourceRequestResponse) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);
  SetActiveSource();

  EXPECT_CALL(*cec_fd_mock_, WriteWatch());
  EXPECT_CALL(*cec_fd_mock_, ReceiveMessage(_))
      .WillOnce(Invoke([](struct cec_msg* msg) {
        cec_msg_init(msg, CEC_LOG_ADDR_TV, CEC_LOG_ADDR_BROADCAST);
        cec_msg_request_active_source(msg, 0);
        return true;
      }));
  // Read the active source request.
  event_callback_.Run(CecFd::EventType::kRead);

  // Let the object write response.
  event_callback_.Run(CecFd::EventType::kWrite);

  EXPECT_EQ(kLogicalAddress, cec_msg_initiator(&sent_message_));
  EXPECT_EQ(CEC_LOG_ADDR_BROADCAST, cec_msg_destination(&sent_message_));
  EXPECT_EQ(CEC_MSG_ACTIVE_SOURCE, cec_msg_opcode(&sent_message_));
  uint16_t address;
  cec_ops_active_source(&sent_message_, &address);
  EXPECT_EQ(kPhysicalAddress, address);
}

TEST_F(CecDeviceTest, TestActiveSourceBrodcastHandling) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);
  SetActiveSource();

  // After receiving active source request broadcast, we should stop
  // to be active source.
  EXPECT_CALL(*cec_fd_mock_, ReceiveMessage(_))
      .WillOnce(Invoke([](struct cec_msg* msg) {
        cec_msg_init(msg, kOtherLogicalAddress, CEC_LOG_ADDR_BROADCAST);
        cec_msg_active_source(msg, kPhysicalAddress + 1);
        return true;
      }));
  // Read the active source request.
  event_callback_.Run(CecFd::EventType::kRead);

  // We will send an active source request...
  EXPECT_CALL(*cec_fd_mock_, ReceiveMessage(_))
      .WillOnce(Invoke([](struct cec_msg* msg) {
        cec_msg_init(msg, CEC_MSG_ACTIVE_SOURCE, CEC_LOG_ADDR_BROADCAST);
        cec_msg_active_source(msg, kPhysicalAddress + 1);
        return true;
      }));
  // which should be ignored now.
  EXPECT_CALL(*cec_fd_mock_, WriteWatch()).Times(0);
  // Read the active source request.
  event_callback_.Run(CecFd::EventType::kRead);
}

TEST_F(CecDeviceTest, TestGetDevicePowerStatus) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  EXPECT_CALL(*cec_fd_mock_, ReceiveMessage(_))
      .WillOnce(Invoke([](struct cec_msg* msg) {
        cec_msg_init(msg, kOtherLogicalAddress, kLogicalAddress);
        cec_msg_give_device_power_status(msg, 0);
        return true;
      }));
  EXPECT_CALL(*cec_fd_mock_, WriteWatch());
  // Read the request in.
  event_callback_.Run(CecFd::EventType::kRead);

  // Make the device respond.
  event_callback_.Run(CecFd::EventType::kWrite);

  // Verify the response.
  EXPECT_EQ(kLogicalAddress, cec_msg_initiator(&sent_message_));
  EXPECT_EQ(kOtherLogicalAddress, cec_msg_destination(&sent_message_));
  EXPECT_EQ(CEC_MSG_REPORT_POWER_STATUS, cec_msg_opcode(&sent_message_));
  uint8_t power_status;
  cec_ops_report_power_status(&sent_message_, &power_status);
  EXPECT_EQ(CEC_OP_POWER_STATUS_ON, power_status);
}

TEST_F(CecDeviceTest, TestFeatureAbortResponse) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  // All others, not explicitly supported messages should be responded with
  // feature abort, let's test it with 'record off' request.
  EXPECT_CALL(*cec_fd_mock_, ReceiveMessage(_))
      .WillOnce(Invoke([](struct cec_msg* msg) {
        cec_msg_init(msg, kOtherLogicalAddress, kLogicalAddress);
        cec_msg_record_off(msg, 1);
        return true;
      }));

  EXPECT_CALL(*cec_fd_mock_, WriteWatch());
  // Read the request in.
  event_callback_.Run(CecFd::EventType::kRead);

  // Make the object send the answer.
  event_callback_.Run(CecFd::EventType::kWrite);

  EXPECT_EQ(kLogicalAddress, cec_msg_initiator(&sent_message_));
  EXPECT_EQ(kOtherLogicalAddress, cec_msg_destination(&sent_message_));
  EXPECT_EQ(CEC_MSG_FEATURE_ABORT, cec_msg_opcode(&sent_message_));
}

TEST_F(CecDeviceTest, TestEventReadFailureDisablesDevice) {
  Init();

  // Object should enter disabled state when event read happens.
  EXPECT_CALL(*cec_fd_mock_, CecFdDestructorCalled());
  // Fail event read.
  EXPECT_CALL(*cec_fd_mock_, ReceiveEvent(_)).WillOnce(Return(false));
  event_callback_.Run(CecFd::EventType::kPriorityRead);

  // Verify that the FD has been destroyed at this point, i.e.
  // object has entered disabled state.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(cec_fd_mock_));
}

TEST_F(CecDeviceTest, TestReadFailureDisablesDevice) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  // Object should enter disabled state when event read happens.
  EXPECT_CALL(*cec_fd_mock_, CecFdDestructorCalled());
  // Fail read.
  EXPECT_CALL(*cec_fd_mock_, ReceiveMessage(_)).WillOnce(Return(false));
  event_callback_.Run(CecFd::EventType::kRead);

  // The FD should be destroyed at this point.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(cec_fd_mock_));
}

TEST_F(CecDeviceTest, TestFailureToSetWriteWatchDisablesDevice) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  // Object should enter disabled state when write watch failed.
  EXPECT_CALL(*cec_fd_mock_, WriteWatch()).WillOnce(Return(false));

  // Set e.g. standby request, to make the device want to start writing.
  device_->SetStandBy();

  // The FD should be destroyed at this point.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(cec_fd_mock_));
}

TEST_F(CecDeviceTest, TestFailureToSendMessageDisablesDevice) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  // Object should enter disabled state when it fails to write out image view
  // on message.
  EXPECT_CALL(*cec_fd_mock_, CecFdDestructorCalled());

  EXPECT_CALL(*cec_fd_mock_, TransmitMessage(_))
      .WillOnce(Return(CecFd::TransmitResult::kError));
  device_->SetWakeUp();
  event_callback_.Run(CecFd::EventType::kWrite);

  // The FD should be destroyed at this point.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(cec_fd_mock_));
}

TEST_F(CecDeviceTest, TestErrorBusyRetries) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  // Object should retry
  EXPECT_CALL(*cec_fd_mock_, WriteWatch())
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*cec_fd_mock_, TransmitMessage(_))
      .Times(2)
      .WillRepeatedly(DoAll(SaveArgPointee<0>(&sent_message_),
                            Return(CecFd::TransmitResult::kBusy)));
  device_->SetWakeUp();
  event_callback_.Run(CecFd::EventType::kWrite);

  EXPECT_EQ(CEC_MSG_IMAGE_VIEW_ON, cec_msg_opcode(&sent_message_));
  sent_message_ = {};

  event_callback_.Run(CecFd::EventType::kWrite);
  EXPECT_EQ(CEC_MSG_IMAGE_VIEW_ON, cec_msg_opcode(&sent_message_));
}

TEST_F(CecDeviceTest, TestGetTvStatus) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  TvPowerStatus power_status = kTvPowerStatusUnknown;

  CecDevice::GetTvPowerStatusCallback callback =
      base::Bind(Copy, &power_status);
  device_->GetTvPowerStatus(callback);

  EXPECT_CALL(*cec_fd_mock_, TransmitMessage(_))
      .WillOnce(Invoke([](struct cec_msg* msg) {
        msg->sequence = 1;
        return CecFd::TransmitResult::kOk;
      }));
  event_callback_.Run(CecFd::EventType::kWrite);

  EXPECT_CALL(*cec_fd_mock_, ReceiveMessage(_))
      .WillOnce(Invoke([](struct cec_msg* msg) {
        cec_msg_init(msg, CEC_LOG_ADDR_TV, kLogicalAddress);
        cec_msg_report_power_status(msg, CEC_OP_POWER_STATUS_ON);
        msg->sequence = 1;
        msg->tx_status = CEC_TX_STATUS_OK;
        msg->rx_status = CEC_RX_STATUS_OK;
        return true;
      }));
  // Read the request in.
  event_callback_.Run(CecFd::EventType::kRead);

  EXPECT_EQ(kTvPowerStatusOn, power_status);
}

TEST_F(CecDeviceTest, TestGetTvStatusOnDisconnect) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  TvPowerStatus power_status = kTvPowerStatusUnknown;
  device_->GetTvPowerStatus(base::Bind(Copy, &power_status));

  SendStateUpdateEvent(CEC_PHYS_ADDR_INVALID, CEC_LOG_ADDR_INVALID);
  EXPECT_EQ(kTvPowerStatusAdapterNotConfigured, power_status);
}

TEST_F(CecDeviceTest, TestGetTvStatusError) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  TvPowerStatus power_status = kTvPowerStatusUnknown;
  EXPECT_CALL(*cec_fd_mock_, WriteWatch()).WillOnce(Return(false));

  device_->GetTvPowerStatus(base::Bind(Copy, &power_status));
  EXPECT_EQ(kTvPowerStatusError, power_status);
}

TEST_F(CecDeviceTest, TestMessageSendingWhenNoLogicalAddressIsConfigured) {
  Init();

  ON_CALL(*cec_fd_mock_, GetLogicalAddresses(_))
      .WillByDefault(Invoke([](struct cec_log_addrs* address) {
        address->num_log_addrs = 0;
        return true;
      }));

  ON_CALL(*cec_fd_mock_, SetLogicalAddresses(_)).WillByDefault(Return(true));

  // Set the object into a state where we have a valid physical address but no
  // logical one, yet.
  SendStateUpdateEvent(kPhysicalAddress, 0);

  // Ask to send a standby request.
  device_->SetStandBy();

  // Provide a logical address now.
  SendStateUpdateEvent(kPhysicalAddress, kLogicalAddressMask);

  ConfigureTVAddress(CEC_LOG_ADDR_TV);

  // Tell the object that the fd is ready to be written to.
  event_callback_.Run(CecFd::EventType::kWrite);

  // Verify that the messsage that has been sent has a proper address.
  EXPECT_EQ(kLogicalAddress, cec_msg_initiator(&sent_message_));
}

extern const size_t kCecDeviceMaxTxQueueSize;

TEST_F(CecDeviceTest, TestMaxTxQueueSize) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  ON_CALL(*cec_fd_mock_, WriteWatch()).WillByDefault(Return(true));

  TvPowerStatus power_status;
  for (size_t i = 0; i < kCecDeviceMaxTxQueueSize; i++) {
    device_->GetTvPowerStatus(base::Bind(Copy, &power_status));
  }

  // The output queue is full now, should respond immediately with an error.
  TvPowerStatus power_status_error = kTvPowerStatusUnknown;
  device_->GetTvPowerStatus(base::Bind(Copy, &power_status_error));
  EXPECT_EQ(kTvPowerStatusError, power_status_error);
}

TEST_F(CecDeviceTest, TestTvProbingFirstProbeSuceedes) {
  Init();
  Connect();

  device_->SetStandBy();

  event_callback_.Run(CecFd::EventType::kWrite);
  event_callback_.Run(CecFd::EventType::kWrite);

  struct cec_msg msg;
  cec_msg_init(&msg, 0, CEC_LOG_ADDR_BROADCAST);
  cec_msg_report_physical_addr(&msg, 0, CEC_OP_PRIM_DEVTYPE_TV);
  msg.sequence = 1;
  ReadMessageIn(msg);

  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_TV, CEC_MSG_STANDBY);
}

TEST_F(CecDeviceTest, TestTvProbingSecondProbeSuceeds) {
  Init();
  Connect();

  device_->SetStandBy();

  // Two 'ticks' are needed for the the CecDevice to send an initial
  // 'give physical address' message.
  event_callback_.Run(CecFd::EventType::kWrite);
  event_callback_.Run(CecFd::EventType::kWrite);

  struct cec_msg msg;
  cec_msg_init(&msg, kLogicalAddress, 0);
  cec_msg_give_physical_addr(&msg, 0);
  msg.sequence = 1;
  msg.tx_status = CEC_TX_STATUS_NACK;
  ReadMessageIn(msg);

  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_SPECIFIC,
                      CEC_MSG_GIVE_PHYSICAL_ADDR);

  cec_msg_init(&msg, CEC_LOG_ADDR_SPECIFIC, CEC_LOG_ADDR_BROADCAST);
  cec_msg_report_physical_addr(&msg, 0, CEC_OP_PRIM_DEVTYPE_TV);
  msg.sequence = 1;
  ReadMessageIn(msg);

  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_SPECIFIC, CEC_MSG_STANDBY);
}

TEST_F(CecDeviceTest, TestTvProbingBroadcastTerminatesProbing) {
  Init();
  Connect();

  device_->SetWakeUp();

  // Two 'ticks' are needed for the the CecDevice to send an initial
  // 'give physical address' message.
  event_callback_.Run(CecFd::EventType::kWrite);
  event_callback_.Run(CecFd::EventType::kWrite);

  struct cec_msg msg;

  // Unsolicited broadcast.
  cec_msg_init(&msg, CEC_LOG_ADDR_SPECIFIC, kLogicalAddress);
  cec_msg_report_physical_addr(&msg, 0, CEC_OP_PRIM_DEVTYPE_TV);
  ReadMessageIn(msg);

  // Response to the query.
  cec_msg_init(&msg, kLogicalAddress, 0);
  cec_msg_give_physical_addr(&msg, 0);
  msg.sequence = 1;
  msg.tx_status = CEC_TX_STATUS_NACK;
  ReadMessageIn(msg);

  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_SPECIFIC,
                      CEC_MSG_IMAGE_VIEW_ON);
}

TEST_F(CecDeviceTest, TestTvProbingFirstResponseFromWrongPhysicalAddress) {
  Init();
  Connect();

  device_->SetWakeUp();

  // Two 'ticks' are needed for the the CecDevice to send an initial
  // 'give physical address' message.
  event_callback_.Run(CecFd::EventType::kWrite);
  event_callback_.Run(CecFd::EventType::kWrite);

  struct cec_msg msg;
  cec_msg_init(&msg, 0, 0);
  cec_msg_report_physical_addr(&msg, 1, CEC_OP_PRIM_DEVTYPE_TV);
  msg.sequence = 1;
  ReadMessageIn(msg);

  // We should see another probe.
  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_SPECIFIC,
                      CEC_MSG_GIVE_PHYSICAL_ADDR);
}

TEST_F(CecDeviceTest, TestTvProbingAllRequestsFail) {
  Init();
  Connect();

  struct cec_msg fail_msg;
  cec_msg_init(&fail_msg, kLogicalAddress, 0);
  cec_msg_give_physical_addr(&fail_msg, 0);
  fail_msg.sequence = 1;
  fail_msg.tx_status = CEC_TX_STATUS_NACK;

  device_->SetStandBy();

  // Two 'ticks' are needed for the the CecDevice to send an initial
  // 'give physical address' message.
  event_callback_.Run(CecFd::EventType::kWrite);
  event_callback_.Run(CecFd::EventType::kWrite);

  ReadMessageIn(fail_msg);
  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_SPECIFIC,
                      CEC_MSG_GIVE_PHYSICAL_ADDR);

  ReadMessageIn(fail_msg);
  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_TV, CEC_MSG_STANDBY);
}

TEST_F(CecDeviceTest, TestTvProbingAllSendsFail) {
  Init();
  Connect();

  device_->SetStandBy();

  // Two 'ticks' are needed for the the CecDevice to send an initial
  // 'give physical address' message.
  event_callback_.Run(CecFd::EventType::kWrite);

  EXPECT_CALL(*cec_fd_mock_, TransmitMessage(_))
      .WillOnce(Return(CecFd::TransmitResult::kNoNet));
  event_callback_.Run(CecFd::EventType::kWrite);

  EXPECT_CALL(*cec_fd_mock_, TransmitMessage(_))
      .WillOnce(Return(CecFd::TransmitResult::kBusy));
  event_callback_.Run(CecFd::EventType::kWrite);

  EXPECT_CALL(*cec_fd_mock_, TransmitMessage(_))
      .WillOnce(Return(CecFd::TransmitResult::kNoNet));
  event_callback_.Run(CecFd::EventType::kWrite);

  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_TV, CEC_MSG_STANDBY);
}

TEST_F(CecDeviceTest, TestTvProbingSendNonrecoverableError) {
  Init();
  Connect();

  device_->SetStandBy();
  // Two 'ticks' are needed for the the CecDevice to send an initial
  // 'give physical address' message.
  event_callback_.Run(CecFd::EventType::kWrite);

  EXPECT_CALL(*cec_fd_mock_, CecFdDestructorCalled());
  EXPECT_CALL(*cec_fd_mock_, TransmitMessage(_))
      .WillOnce(Return(CecFd::TransmitResult::kError));
  event_callback_.Run(CecFd::EventType::kWrite);
  // Verify that the fd has been destroyed at this point, i.e.
  // object has entered disabled state.
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(cec_fd_mock_));
}

TEST_F(CecDeviceTest, TestApiRequestRetriggersProbing) {
  Init();
  Connect();

  device_->SetStandBy();

  // Two 'ticks' are needed for the the CecDevice to send an initial
  // 'give physical address' message.
  event_callback_.Run(CecFd::EventType::kWrite);
  event_callback_.Run(CecFd::EventType::kWrite);

  struct cec_msg msg;
  cec_msg_init(&msg, kLogicalAddress, 0);
  cec_msg_give_physical_addr(&msg, 0);
  msg.sequence = 1;
  msg.tx_status = CEC_TX_STATUS_NACK;

  ReadMessageIn(msg);
  // Ignore the sent out message.
  event_callback_.Run(CecFd::EventType::kWrite);

  ReadMessageIn(msg);
  // Ignore the sent out message.
  event_callback_.Run(CecFd::EventType::kWrite);

  // Another request, should trigger requery.
  device_->SetStandBy();

  // First spurious write.
  event_callback_.Run(CecFd::EventType::kWrite);
  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_TV,
                      CEC_MSG_GIVE_PHYSICAL_ADDR);
}

TEST_F(CecDeviceTest, TestSendingToTVFailsReproesAddress) {
  Init();
  ConnectAndConfigureTVAddress(CEC_LOG_ADDR_TV);

  device_->SetStandBy();

  struct cec_msg msg;
  cec_msg_init(&msg, kLogicalAddress, 0);
  cec_msg_standby(&msg);
  msg.sequence = 1;
  msg.tx_status = CEC_TX_STATUS_NACK;
  ReadMessageIn(msg);

  // We should start off by reprobing TV address.
  device_->SetStandBy();

  // Two 'ticks' are needed for the the CecDevice to send an initial
  // 'give physical address' message.
  event_callback_.Run(CecFd::EventType::kWrite);
  SendAndCheckMessage(kLogicalAddress, CEC_LOG_ADDR_TV,
                      CEC_MSG_GIVE_PHYSICAL_ADDR);
}

}  // namespace cecservice
