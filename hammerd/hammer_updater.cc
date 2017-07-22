// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// hammerd - A daemon to update the firmware of Hammer

#include "hammerd/hammer_updater.h"

#include <unistd.h>

#include <base/logging.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>

namespace hammerd {

HammerUpdater::HammerUpdater(const std::string& image)
    : HammerUpdater(
          image,
          std::unique_ptr<FirmwareUpdaterInterface>(new FirmwareUpdater())) {}

HammerUpdater::HammerUpdater(
    const std::string& image,
    std::unique_ptr<FirmwareUpdaterInterface> fw_updater)
    : image_(image), fw_updater_(std::move(fw_updater)) {}

bool HammerUpdater::Run() {
  LOG(INFO) << "Load and validate the image.";
  if (!fw_updater_->LoadImage(image_)) {
    LOG(ERROR) << "Failed to load image.";
    return false;
  }

  constexpr unsigned int kMaximumRunCount = 10;
  // Time it takes hammer to reset or jump to RW, before being
  // available for the next USB connection.
  constexpr unsigned int kResetTimeMs = 100;
  bool post_rw_jump = false;

  for (int run_count = 0; run_count < kMaximumRunCount; ++run_count) {
    HammerUpdater::RunStatus status = HammerUpdater::RunStatus::kFatalError;

    if (fw_updater_->TryConnectUSB()) {
      status = RunOnce(post_rw_jump);
    } else {
      LOG(ERROR) << "Failed to connect USB.";
    }
    post_rw_jump = false;

    switch (status) {
      case HammerUpdater::RunStatus::kNoUpdate:
        LOG(INFO) << "Hammer does not need to update.";
        fw_updater_->CloseUSB();
        return true;

      case HammerUpdater::RunStatus::kFatalError:
        LOG(ERROR) << "Hammer encountered a fatal error!";
        fw_updater_->CloseUSB();
        return false;

      case HammerUpdater::RunStatus::kNeedReset:
        LOG(INFO) << "Reset hammer and run again. run_count=" << run_count;
        fw_updater_->SendSubcommand(UpdateExtraCommand::kImmediateReset);
        fw_updater_->CloseUSB();
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(kResetTimeMs));
        continue;

      case HammerUpdater::RunStatus::kNeedJump:
        post_rw_jump = true;
        LOG(INFO) << "Jump to RW and run again. run_count=" << run_count;
        fw_updater_->SendSubcommand(UpdateExtraCommand::kJumpToRW);
        fw_updater_->CloseUSB();
        // TODO(kitching): Make RW jumps more robust by polling until
        // the jump completes (or fails).
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(kResetTimeMs));
        continue;

      default:
        LOG(ERROR) << "Unknown RunStatus: " << static_cast<int>(status);
        fw_updater_->CloseUSB();
        return false;
    }
  }

  LOG(ERROR) << "Maximum run count exceeded (" << kMaximumRunCount
             << ")! Shutdown Hammerd.";
  return false;
}

HammerUpdater::RunStatus HammerUpdater::RunOnce(const bool post_rw_jump) {
  // The first time we use SendFirstPDU it is to gather information about
  // hammer's running EC. We should use SendDone right away to get the EC
  // back into a state where we can send a subcommand.
  if (!fw_updater_->SendFirstPDU()) {
    LOG(ERROR) << "Failed to send the first PDU.";
    return HammerUpdater::RunStatus::kNeedReset;
  }
  fw_updater_->SendDone();

  LOG(INFO) << "### CURRENT SECTION: "
            << ToString(fw_updater_->CurrentSection()) << " ###";

  // ********************** RW **********************
  // If EC already entered the RW section, then check if RW needs updating.
  // If an update is needed, request a hammer reset. Let the next invocation
  // of Run handle the update.
  if (fw_updater_->CurrentSection() == SectionName::RW) {
    if (fw_updater_->NeedsUpdate(SectionName::RW)) {
      LOG(INFO) << "RW section needs update.";
      return HammerUpdater::RunStatus::kNeedReset;
    }
    PostRWProcess();
    return HammerUpdater::RunStatus::kNoUpdate;
  }

  // ********************** RO **********************
  // Current section is now guaranteed to be RO.  If hammer:
  //   (1) failed to jump to RW after the last run; or
  //   (2) RW section needs updating,
  // then continue with the update procedure.
  if (post_rw_jump || fw_updater_->NeedsUpdate(SectionName::RW)) {
    // If we have just finished a jump to RW, but we're still in RO, then
    // we should log the failure.
    if (post_rw_jump) {
      LOG(ERROR) << "Failed to jump to RW. Need to update RW section.";
    }

    // EC is still running in RO section. Send "Stay in RO" command before
    // continuing.
    LOG(INFO) << "Sending stay in RO command.";
    if (!fw_updater_->SendSubcommand(UpdateExtraCommand::kStayInRO)) {
      LOG(ERROR) << "Failed to stay in RO.";
      return HammerUpdater::RunStatus::kNeedReset;
    }

    if (fw_updater_->IsSectionLocked(SectionName::RW)) {
      LOG(INFO) << "Unlock RW section, and reset EC.";
      fw_updater_->UnLockSection(SectionName::RW);
      return HammerUpdater::RunStatus::kNeedReset;
    }

    // Now RW section needs an update, and it is not locked. Let's update!
    bool ret = fw_updater_->TransferImage(SectionName::RW);
    LOG(INFO) << "RW update " << (ret ? "passed." : "failed.");
    return HammerUpdater::RunStatus::kNeedReset;
  }

  LOG(INFO) << "No need to update RW. Jump to RW section.";
  return HammerUpdater::RunStatus::kNeedJump;
}

void HammerUpdater::PostRWProcess() {
  LOG(INFO) << "Start the process after entering RW section.";
  // TODO(akahuang): Update RO section in dogfood mode.
  // TODO(akahuang): Update trackpad FW.
  // TODO(akahuang): Pairing.
  // TODO(akahuang): Rollback increment.
}

}  // namespace hammerd
