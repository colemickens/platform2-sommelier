// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hammerd/hammer_updater.h"
#include "hammerd/mock_update_fw.h"

using testing::_;
using testing::Assign;
using testing::AtLeast;
using testing::DoAll;
using testing::Exactly;
using testing::InSequence;
using testing::Return;
using testing::ReturnPointee;

namespace hammerd {

ACTION_P(Increment, n) {
  ++(*n);
}
ACTION_P(Decrement, n) {
  --(*n);
}

// The class is used to test the top-level method: Run(), so we mock
// other methods.
class MockHammerUpdater : public HammerUpdater {
 public:
  using HammerUpdater::HammerUpdater;
  ~MockHammerUpdater() override = default;

  MOCK_METHOD1(RunOnce, RunStatus(const bool post_rw_jump));
};

class HammerUpdaterTest : public testing::Test {
 public:
  virtual void SetFwUpdater() = 0;

  void SetUp() override {
    SetFwUpdater();
    // By default, expect no USB connections to be made. This can
    // be overridden by a call to ExpectUSBConnections.
    usb_connection_count_ = 0;
    EXPECT_CALL(*fw_updater_, TryConnectUSB()).Times(0);
    EXPECT_CALL(*fw_updater_, CloseUSB()).Times(0);
  }

  void TearDown() override { ASSERT_EQ(usb_connection_count_, 0); }

  void ExpectUSBConnections(const testing::Cardinality count) {
    // Checked in TearDown.
    EXPECT_CALL(*fw_updater_, TryConnectUSB())
        .Times(count)
        .WillRepeatedly(DoAll(Increment(&usb_connection_count_), Return(true)));
    EXPECT_CALL(*fw_updater_, CloseUSB())
        .Times(count)
        .WillRepeatedly(DoAll(Decrement(&usb_connection_count_), Return()));
  }

 protected:
  MockFirmwareUpdater* fw_updater_;
  std::string image_ = "MOCK IMAGE";
  int usb_connection_count_;
};

class HammerUpdaterFlowTest : public HammerUpdaterTest {
 public:
  void SetFwUpdater() override {
    // MockHammerUpdater mocks out RunOnce so that Run can be tested.
    // FirmwareUpdater is also mocked out.
    hammer_updater_.reset(new MockHammerUpdater{
        image_, base::MakeUnique<MockFirmwareUpdater>()});
    fw_updater_ =
        static_cast<MockFirmwareUpdater*>(hammer_updater_->fw_updater_.get());
  }

 protected:
  std::unique_ptr<MockHammerUpdater> hammer_updater_;
};

class HammerUpdaterFullTest : public HammerUpdaterTest {
 public:
  void SetFwUpdater() override {
    // Use the normal HammerUpdater class.  We only mock out FirmwareUpdater.
    hammer_updater_.reset(new HammerUpdater{
        image_, base::MakeUnique<MockFirmwareUpdater>()});
    fw_updater_ =
        static_cast<MockFirmwareUpdater*>(hammer_updater_->fw_updater_.get());
  }

 protected:
  std::unique_ptr<HammerUpdater> hammer_updater_;
};

// Failed to load image.
TEST_F(HammerUpdaterFlowTest, Run_LoadImageFailed) {
  EXPECT_CALL(*fw_updater_, LoadImage(image_)).WillOnce(Return(false));
  EXPECT_CALL(*fw_updater_, TryConnectUSB()).Times(0);
  EXPECT_CALL(*hammer_updater_, RunOnce(_)).Times(0);

  ASSERT_FALSE(hammer_updater_->Run());
}

// Sends reset command if RunOnce returns kNeedReset.
TEST_F(HammerUpdaterFlowTest, Run_AlwaysReset) {
  EXPECT_CALL(*fw_updater_, LoadImage(image_)).WillOnce(Return(true));
  EXPECT_CALL(*hammer_updater_, RunOnce(false))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(HammerUpdater::RunStatus::kNeedReset));
  EXPECT_CALL(*fw_updater_, SendSubcommand(UpdateExtraCommand::kImmediateReset))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));

  ExpectUSBConnections(AtLeast(1));
  ASSERT_FALSE(hammer_updater_->Run());
}

// A fatal error occurred during update.
TEST_F(HammerUpdaterFlowTest, Run_FatalError) {
  EXPECT_CALL(*fw_updater_, LoadImage(image_)).WillOnce(Return(true));
  EXPECT_CALL(*hammer_updater_, RunOnce(false))
      .WillOnce(Return(HammerUpdater::RunStatus::kFatalError));

  ExpectUSBConnections(AtLeast(1));
  ASSERT_FALSE(hammer_updater_->Run());
}

// After three attempts, Run reports no update needed.
TEST_F(HammerUpdaterFlowTest, Run_Reset3Times) {
  EXPECT_CALL(*fw_updater_, LoadImage(image_)).WillOnce(Return(true));
  EXPECT_CALL(*hammer_updater_, RunOnce(false))
      .WillOnce(Return(HammerUpdater::RunStatus::kNeedReset))
      .WillOnce(Return(HammerUpdater::RunStatus::kNeedReset))
      .WillOnce(Return(HammerUpdater::RunStatus::kNeedReset))
      .WillRepeatedly(Return(HammerUpdater::RunStatus::kNoUpdate));
  EXPECT_CALL(*fw_updater_, SendSubcommand(UpdateExtraCommand::kImmediateReset))
      .Times(3)
      .WillRepeatedly(Return(true));

  ExpectUSBConnections(Exactly(4));
  ASSERT_EQ(hammer_updater_->Run(), true);
}

// Return false if the layout of the firmware is changed.
// Condition:
//   1. The current section is Invalid.
TEST_F(HammerUpdaterFullTest, Run_InvalidSection) {
  EXPECT_CALL(*fw_updater_, LoadImage(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::Invalid));

  {
    InSequence dummy;
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, SendDone()).WillOnce(Return());
  }

  ExpectUSBConnections(AtLeast(1));
  ASSERT_EQ(hammer_updater_->Run(), false);
}

// Update the RW after JUMP_TO_RW failed.
// Condition:
//   1. In RO section.
//   2. RW does not need update.
//   3. Fails to jump to RW due to invalid signature.
TEST_F(HammerUpdaterFullTest, Run_UpdateRWAfterJumpToRWFailed) {
  SectionName current_section = SectionName::RO;

  EXPECT_CALL(*fw_updater_, LoadImage(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*fw_updater_, IsSectionLocked(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*fw_updater_, CurrentSection())
      .WillRepeatedly(ReturnPointee(&current_section));

  {
    InSequence dummy;

    // First round: RW does not need update.  Attempt to jump to RW.
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, SendDone()).WillOnce(Return());
    EXPECT_CALL(*fw_updater_, SendSubcommand(UpdateExtraCommand::kJumpToRW))
        .WillOnce(Return(true));

    // Second round: Jump to RW fails, so update RW.
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, SendDone()).WillOnce(Return());
    EXPECT_CALL(*fw_updater_, SendSubcommand(UpdateExtraCommand::kStayInRO))
        .WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, TransferImage(SectionName::RW))
        .WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_,
                SendSubcommand(UpdateExtraCommand::kImmediateReset))
        .WillOnce(Return(true));

    // Third round: Again attempt to jump to RW.
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, SendDone()).WillOnce(Return());
    EXPECT_CALL(*fw_updater_, SendSubcommand(UpdateExtraCommand::kJumpToRW))
        .WillOnce(
            DoAll(Assign(&current_section, SectionName::RW), Return(true)));

    // Fourth round: Check that jumping to RW was successful.
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, SendDone()).WillOnce(Return());
  }

  ExpectUSBConnections(AtLeast(1));
  ASSERT_EQ(hammer_updater_->Run(), true);
}

// Update the RW and continue.
// Condition:
//   1. In RO section.
//   2. RW needs update.
//   3. RW is not locked.
TEST_F(HammerUpdaterFullTest, RunOnce_UpdateRW) {
  EXPECT_CALL(*fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RO));
  EXPECT_CALL(*fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*fw_updater_, IsSectionLocked(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, SendSubcommand(UpdateExtraCommand::kStayInRO))
        .WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, TransferImage(SectionName::RW))
        .WillOnce(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kNeedReset);
}

// Unlock the RW and reset.
// Condition:
//   1. In RO section.
//   2. RW needs update.
//   3. RW is locked.
TEST_F(HammerUpdaterFullTest, RunOnce_UnlockRW) {
  EXPECT_CALL(*fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RO));
  EXPECT_CALL(*fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*fw_updater_, IsSectionLocked(SectionName::RW))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, SendSubcommand(UpdateExtraCommand::kStayInRO))
        .WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, UnLockSection(SectionName::RW))
        .WillRepeatedly(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kNeedReset);
}

// Jump to RW.
// Condition:
//   1. In RO section.
//   2. RW does not need update.
TEST_F(HammerUpdaterFullTest, RunOnce_JumpToRW) {
  EXPECT_CALL(*fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RO));
  EXPECT_CALL(*fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kNeedJump);
}

// Complete RW jump.
// Condition:
//   1. In RW section.
//   2. RW jump flag is set.
TEST_F(HammerUpdaterFullTest, RunOnce_CompleteRWJump) {
  EXPECT_CALL(*fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RW));
  EXPECT_CALL(*fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;

    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(true),
            HammerUpdater::RunStatus::kNoUpdate);
}

// Keep in RW.
// Condition:
//   1. In RW section.
//   2. RW does not need update.
TEST_F(HammerUpdaterFullTest, RunOnce_KeepInRW) {
  EXPECT_CALL(*fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RW));
  EXPECT_CALL(*fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kNoUpdate);
}

// Reset to RO.
// Condition:
//   1. In RW section.
//   2. RW needs update.
TEST_F(HammerUpdaterFullTest, RunOnce_ResetToRO) {
  EXPECT_CALL(*fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RW));
  EXPECT_CALL(*fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kNeedReset);
}
}  // namespace hammerd
