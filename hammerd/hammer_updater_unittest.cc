// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The calling structure of HammerUpdater:
//   Run() => RunLoop() => RunOnce() => PostRWProcess().
// Since RunLoop only iterately call the Run() method, so we don't test it
// directly. Therefore, we have 3-layer unittests:
//
// - HammerUpdaterFlowTest:
//  - Test the logic of Run(), the interaction with RunOnce().
//  - Mock RunOnce() and data members.
//
// - HammerUpdaterRWTest:
//  - Test the logic of RunOnce(), the interaction with PostRWProcess() and
//    external interfaces (fw_updater, pair_manager, ...etc).
//  - One exception: Test a special sequence that needs to reset 3 times called
//    by Run().
//  - Mock PostRWProcess() and data members.
//
// - HammerUpdaterPostRWTest:
//  - Test the logic of PostRWProcess, the interaction with external interfaces.
//  - Mock all external data members only.

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

class MockRunOnceHammerUpdater : public HammerUpdater {
 public:
  using HammerUpdater::HammerUpdater;
  ~MockRunOnceHammerUpdater() override = default;

  MOCK_METHOD1(RunOnce, RunStatus(const bool post_rw_jump));
};

class MockRWProcessHammerUpdater : public HammerUpdater {
 public:
  using HammerUpdater::HammerUpdater;
  ~MockRWProcessHammerUpdater() override = default;

  MOCK_METHOD0(PostRWProcess, void());
};

template <typename HammerUpdaterType>
class HammerUpdaterTest : public testing::Test {
 public:
  void SetUp() override {
    // Mock out data members.
    hammer_updater_.reset(new HammerUpdaterType{
        image_, base::MakeUnique<MockFirmwareUpdater>())});
    fw_updater_ =
        static_cast<MockFirmwareUpdater*>(hammer_updater_->fw_updater_.get());

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
  std::unique_ptr<HammerUpdaterType> hammer_updater_;
  MockFirmwareUpdater* fw_updater_;
  std::string image_ = "MOCK IMAGE";
  int usb_connection_count_;
};

// We mock RunOnce function here to verify the interaction between Run() and
// RunOnce().
class HammerUpdaterFlowTest
    : public HammerUpdaterTest<MockRunOnceHammerUpdater> {};
// We mock PostRWProcess function here to verify the flow of RW section
// updating.
class HammerUpdaterRWTest
    : public HammerUpdaterTest<MockRWProcessHammerUpdater> {};
// We only mock the data members to verify the PostRWProcess() function.
class HammerUpdaterPostRWTest : public HammerUpdaterTest<HammerUpdater> {};

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
TEST_F(HammerUpdaterRWTest, RunOnce_InvalidSection) {
  EXPECT_CALL(*fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::Invalid));

  {
    InSequence dummy;
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, SendDone()).WillOnce(Return());
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kInvalidFirmware);
}

// Update the RW after JUMP_TO_RW failed.
// Condition:
//   1. In RO section.
//   2. RW does not need update.
//   3. Fails to jump to RW due to invalid signature.
TEST_F(HammerUpdaterRWTest, Run_UpdateRWAfterJumpToRWFailed) {
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

    // Fourth round: Check that jumping to RW was successful, and that
    // PostRWProcessing is called.
    EXPECT_CALL(*fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*fw_updater_, SendDone()).WillOnce(Return());
    EXPECT_CALL(*hammer_updater_, PostRWProcess());
  }

  ExpectUSBConnections(AtLeast(1));
  ASSERT_EQ(hammer_updater_->Run(), true);
}

// Update the RW and continue.
// Condition:
//   1. In RO section.
//   2. RW needs update.
//   3. RW is not locked.
TEST_F(HammerUpdaterRWTest, RunOnce_UpdateRW) {
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
TEST_F(HammerUpdaterRWTest, RunOnce_UnlockRW) {
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
TEST_F(HammerUpdaterRWTest, RunOnce_JumpToRW) {
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
TEST_F(HammerUpdaterRWTest, RunOnce_CompleteRWJump) {
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
TEST_F(HammerUpdaterRWTest, RunOnce_KeepInRW) {
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
TEST_F(HammerUpdaterRWTest, RunOnce_ResetToRO) {
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
