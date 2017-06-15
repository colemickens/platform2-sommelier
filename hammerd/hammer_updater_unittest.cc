// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hammerd/hammer_updater.h"
#include "hammerd/mock_update_fw.h"

using testing::_;
using testing::Assign;
using testing::AtLeast;
using testing::InSequence;
using testing::Return;
using testing::ReturnPointee;

namespace hammerd {

// The class is used to test the top-level method: Run(), so we mock other
// methods.
class MockHammerUpdater : public HammerUpdater {
 public:
  using HammerUpdater::HammerUpdater;
  ~MockHammerUpdater() override = default;

  MOCK_METHOD1(RunOnce, RunStatus(const bool post_rw_jump));
};

class HammerUpdaterTest : public testing::Test {
 public:
  void SetUp() override {
    mock_fw_updater_.reset(new MockFirmwareUpdater());
    // hammer_updater_ is used to test Run().
    mock_hammer_updater_.reset(new MockHammerUpdater{
        image_, std::shared_ptr<FirmwareUpdaterInterface>(mock_fw_updater_)});
    // hammer_updater_ is used to test other methods.
    hammer_updater_.reset(new HammerUpdater{
        image_, std::shared_ptr<FirmwareUpdaterInterface>(mock_fw_updater_)});
  }

 protected:
  std::unique_ptr<MockHammerUpdater> mock_hammer_updater_;
  std::unique_ptr<HammerUpdater> hammer_updater_;
  std::shared_ptr<MockFirmwareUpdater> mock_fw_updater_;
  std::string image_ = "MOCK IMAGE";
};

// Failed to load image.
TEST_F(HammerUpdaterTest, Run_LoadImageFailed) {
  EXPECT_CALL(*mock_fw_updater_, LoadImage(image_)).WillOnce(Return(false));

  ASSERT_FALSE(mock_hammer_updater_->Run());
}

// Sends reset command if RunOnce returns kNeedReset.
TEST_F(HammerUpdaterTest, Run_AlwaysReset) {
  EXPECT_CALL(*mock_fw_updater_, LoadImage(image_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_hammer_updater_, RunOnce(false))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(HammerUpdater::RunStatus::kNeedReset));
  EXPECT_CALL(*mock_fw_updater_,
              SendSubcommand(UpdateExtraCommand::kImmediateReset, _))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_fw_updater_, CloseUSB()).WillRepeatedly(Return());

  ASSERT_FALSE(mock_hammer_updater_->Run());
}

// A fatal error occurred during update.
TEST_F(HammerUpdaterTest, Run_FatalError) {
  EXPECT_CALL(*mock_fw_updater_, LoadImage(image_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_hammer_updater_, RunOnce(false))
      .WillOnce(Return(HammerUpdater::RunStatus::kFatalError));

  ASSERT_FALSE(mock_hammer_updater_->Run());
}

// After three attempts, Run gives up.
TEST_F(HammerUpdaterTest, Run_Reset3Times) {
  EXPECT_CALL(*mock_fw_updater_, LoadImage(image_)).WillOnce(Return(true));
  EXPECT_CALL(*mock_hammer_updater_, RunOnce(false))
      .WillOnce(Return(HammerUpdater::RunStatus::kNeedReset))
      .WillOnce(Return(HammerUpdater::RunStatus::kNeedReset))
      .WillOnce(Return(HammerUpdater::RunStatus::kNeedReset))
      .WillRepeatedly(Return(HammerUpdater::RunStatus::kNoUpdate));
  EXPECT_CALL(*mock_fw_updater_,
              SendSubcommand(UpdateExtraCommand::kImmediateReset, _))
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_fw_updater_, CloseUSB()).Times(3).WillRepeatedly(Return());

  ASSERT_EQ(mock_hammer_updater_->Run(), true);
}

// Update the RW after JUMP_TO_RW failed.
// Condition:
//   1. In RO section.
//   2. RW does not need update.
//   3. Fails to jump to RW due to invalid signature.
TEST_F(HammerUpdaterTest, Run_UpdateRWAfterJumpToRWFailed) {
  SectionName current_section = SectionName::RO;

  EXPECT_CALL(*mock_fw_updater_, LoadImage(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_fw_updater_, IsSectionLocked(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_fw_updater_, CloseUSB()).WillRepeatedly(Return());
  EXPECT_CALL(*mock_fw_updater_, CurrentSection())
      .WillRepeatedly(ReturnPointee(&current_section));

  {
    InSequence dummy;

    // First round: RW does not need update.  Attempt to jump to RW.
    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendDone()).WillOnce(Return());
    EXPECT_CALL(*mock_fw_updater_,
                SendSubcommand(UpdateExtraCommand::kJumpToRW, _))
        .WillOnce(Return(true));

    // Second round: Jump to RW fails, so update RW.
    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendDone()).WillOnce(Return());
    EXPECT_CALL(*mock_fw_updater_,
                SendSubcommand(UpdateExtraCommand::kStayInRO, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, TransferImage(SectionName::RW))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_,
                SendSubcommand(UpdateExtraCommand::kImmediateReset, _))
        .WillOnce(Return(true));

    // Third round: Again attempt to jump to RW.
    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendDone()).WillOnce(Return());
    EXPECT_CALL(*mock_fw_updater_,
                SendSubcommand(UpdateExtraCommand::kJumpToRW, _))
        .WillOnce(DoAll(Assign(&current_section, SectionName::RW),
                        Return(true)));

    // Fourth round: Check that jumping to RW was successful.
    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendDone()).WillOnce(Return());
  }

  ASSERT_EQ(hammer_updater_->Run(), true);
}

// Update the RW and continue.
// Condition:
//   1. In RO section.
//   2. RW needs update.
//   3. RW is not locked.
TEST_F(HammerUpdaterTest, RunOnce_UpdateRW) {
  EXPECT_CALL(*mock_fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RO));
  EXPECT_CALL(*mock_fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_fw_updater_, IsSectionLocked(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_,
                SendSubcommand(UpdateExtraCommand::kStayInRO, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, TransferImage(SectionName::RW))
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
TEST_F(HammerUpdaterTest, RunOnce_UnlockRW) {
  EXPECT_CALL(*mock_fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RO));
  EXPECT_CALL(*mock_fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_fw_updater_, IsSectionLocked(SectionName::RW))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_,
                SendSubcommand(UpdateExtraCommand::kStayInRO, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, UnLockSection(SectionName::RW))
        .WillRepeatedly(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kNeedReset);
}

// Jump to RW.
// Condition:
//   1. In RO section.
//   2. RW does not need update.
TEST_F(HammerUpdaterTest, RunOnce_JumpToRW) {
  EXPECT_CALL(*mock_fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RO));
  EXPECT_CALL(*mock_fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kNeedJump);
}

// Complete RW jump.
// Condition:
//   1. In RW section.
//   2. RW jump flag is set.
TEST_F(HammerUpdaterTest, RunOnce_CompleteRWJump) {
  EXPECT_CALL(*mock_fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RW));
  EXPECT_CALL(*mock_fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;

    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(true),
            HammerUpdater::RunStatus::kNoUpdate);
}

// Keep in RW.
// Condition:
//   1. In RW section.
//   2. RW does not need update.
TEST_F(HammerUpdaterTest, RunOnce_KeepInRW) {
  EXPECT_CALL(*mock_fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RW));
  EXPECT_CALL(*mock_fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kNoUpdate);
}

// Reset to RO.
// Condition:
//   1. In RW section.
//   2. RW needs update.
TEST_F(HammerUpdaterTest, RunOnce_ResetToRO) {
  EXPECT_CALL(*mock_fw_updater_, CurrentSection())
      .WillRepeatedly(Return(SectionName::RW));
  EXPECT_CALL(*mock_fw_updater_, NeedsUpdate(SectionName::RW))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_fw_updater_, SendDone()).WillRepeatedly(Return());

  {
    InSequence dummy;
    EXPECT_CALL(*mock_fw_updater_, TryConnectUSB()).WillOnce(Return(true));
    EXPECT_CALL(*mock_fw_updater_, SendFirstPDU()).WillOnce(Return(true));
  }

  ASSERT_EQ(hammer_updater_->RunOnce(false),
            HammerUpdater::RunStatus::kNeedReset);
}
}  // namespace hammerd
