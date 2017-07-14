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

HammerUpdater::HammerUpdater(const std::string& image) : image_(image) {}

bool HammerUpdater::Run() {
  LOG(INFO) << "Load and validate the image.";
  if (!fw_updater_.LoadImage(image_)) {
    LOG(ERROR) << "Failed to load image.";
    return false;
  }

  constexpr unsigned int kMaximumResetCount = 10;
  constexpr unsigned int kResetTimeMs = 100;
  for (int reset_count = 0; reset_count < kMaximumResetCount; ++reset_count) {
    HammerUpdater::RunStatus status = RunOnce();
    if (status == HammerUpdater::RunStatus::kNoUpdate) {
      LOG(INFO) << "Hammer does not need to update.";
      return true;
    }
    if (status == HammerUpdater::RunStatus::kFatalError) {
      LOG(ERROR) << "Hammer encountered fatal error!";
      return false;
    }
    if (status == HammerUpdater::RunStatus::kNeedReset) {
      LOG(INFO) << "Reset the hammer and run again: " << reset_count;
      fw_updater_.SendSubcommand(UpdateExtraCommand::kImmediateReset);
      fw_updater_.CloseUSB();
      // Give hammer a chance to reset before we try connecting to it again.
      base::PlatformThread::Sleep(
          base::TimeDelta::FromMilliseconds(kResetTimeMs));
    } else {
      LOG(ERROR) << "Unknown RunStatus: " << static_cast<int>(status);
      return false;
    }
  }
  LOG(ERROR) << "Multiple resets occurred! Shutdown Hammerd.";
  return false;
}

HammerUpdater::RunStatus HammerUpdater::RunOnce() {
  if (!fw_updater_.TryConnectUSB()) {
    LOG(ERROR) << "Failed to connect USB.";
    return HammerUpdater::RunStatus::kFatalError;
  }

  // The first time we use SendFirstPDU it is to gather information about
  // hammer's running EC.  We should use SendDone right away to get the EC
  // back into a state where we can send a subcommand.
  if (!fw_updater_.SendFirstPDU()) {
    LOG(ERROR) << "Failed to send the first PDU.";
    return HammerUpdater::RunStatus::kNeedReset;
  }
  fw_updater_.SendDone();

  LOG(INFO) << "### CURRENT SECTION: "
            << ToString(fw_updater_.CurrentSection()) << " ###";

  // If EC already entered RW section, then check if the RW need update. If so,
  // then reset the hammer and exit. Let the next invocation handle the update.
  if (fw_updater_.CurrentSection() == SectionName::RW) {
    if (fw_updater_.NeedsUpdate(SectionName::RW)) {
      LOG(INFO) << "RW section needs update.";
      return HammerUpdater::RunStatus::kNeedReset;
    }
    return HammerUpdater::RunStatus::kNoUpdate;
  }

  if (!fw_updater_.NeedsUpdate(SectionName::RW)) {
    LOG(INFO) << "No need to update RW. Jump to RW section.";
    // TODO(akahuang): If JUMP_TO_RW failed, then update the RW.
    fw_updater_.SendSubcommand(UpdateExtraCommand::kJumpToRW);
    // TODO(akahuang): Update RO section in dogfood mode.
    // TODO(akahuang): Update trackpad FW.
    // TODO(akahuang): Pairing.
    // TODO(akahuang): Rollback increment.
    return HammerUpdater::RunStatus::kNoUpdate;
  }

  // EC is still running in RO section. Send "Stay in RO" command before
  // continuing.
  if (!fw_updater_.SendSubcommand(UpdateExtraCommand::kStayInRO)) {
    LOG(ERROR) << "Failed to stay in RO.";
    return HammerUpdater::RunStatus::kNeedReset;
  }

  if (fw_updater_.IsSectionLocked(SectionName::RW)) {
    LOG(INFO) << "Unlock RW section, and reset EC.";
    fw_updater_.UnLockSection(SectionName::RW);
    return HammerUpdater::RunStatus::kNeedReset;
  }

  // Now RW section needs an update, and it is not locked. Let's update!
  bool ret = fw_updater_.TransferImage(SectionName::RW);
  LOG(INFO) << "RW image transfer " << (ret ? "completed" : "failed") << ".";
  return HammerUpdater::RunStatus::kNeedReset;
}

}  // namespace hammerd
