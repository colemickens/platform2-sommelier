// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// hammerd - A daemon to update the firmware of Hammer

#include "hammerd/hammer_updater.h"

#include <unistd.h>

#include <memory>
#include <utility>

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>

namespace hammerd {

HammerUpdater::HammerUpdater(const std::string& ec_image,
                             const std::string& touchpad_image,
                             uint16_t vendor_id, uint16_t product_id,
                             int bus, int port)
    : HammerUpdater(
        ec_image,
        touchpad_image,
        std::make_unique<FirmwareUpdater>(
            std::make_unique<UsbEndpoint>(vendor_id, product_id, bus, port)),
        std::make_unique<PairManager>(),
        std::make_unique<DBusWrapper>()) {}

HammerUpdater::HammerUpdater(
    const std::string& ec_image,
    const std::string& touchpad_image,
    std::unique_ptr<FirmwareUpdaterInterface> fw_updater,
    std::unique_ptr<PairManagerInterface> pair_manager,
    std::unique_ptr<DBusWrapperInterface> dbus_wrapper)
    : ec_image_(ec_image),
      touchpad_image_(touchpad_image),
      fw_updater_(std::move(fw_updater)),
      pair_manager_(std::move(pair_manager)),
      dbus_wrapper_(std::move(dbus_wrapper)),
      dbus_notified_(false) {}

bool HammerUpdater::Run() {
  LOG(INFO) << "Load and validate the EC image.";
  if (!fw_updater_->LoadEcImage(ec_image_)) {
    LOG(ERROR) << "Failed to load EC image.";
    return false;
  }

  HammerUpdater::RunStatus status = RunLoop();
  bool ret = (status == HammerUpdater::RunStatus::kNoUpdate);
  WaitUsbReady(status);

  // If we tried to update the firmware, send a signal to notify the updating is
  // finished.
  if (dbus_notified_) {
    dbus_notified_ = false;
    dbus_wrapper_->SendSignal(ret ? kBaseFirmwareUpdateSucceededSignal
                                  : kBaseFirmwareUpdateFailedSignal);
  }
  return ret;
}

HammerUpdater::RunStatus HammerUpdater::RunLoop() {
  constexpr unsigned int kMaximumRunCount = 10;
  // Time it takes hammer to reset or jump to RW, before being
  // available for the next USB connection.
  constexpr unsigned int kResetTimeMs = 100;
  bool post_rw_jump = false;
  bool need_inject_entropy = false;

  HammerUpdater::RunStatus status;
  for (int run_count = 0; run_count < kMaximumRunCount; ++run_count) {
    UsbConnectStatus connect_status = fw_updater_->TryConnectUsb();

    if (connect_status != UsbConnectStatus::kSuccess) {
      LOG(ERROR) << "Failed to connect USB.";
      fw_updater_->CloseUsb();

      if (connect_status == UsbConnectStatus::kUsbPathEmpty) {
        return HammerUpdater::RunStatus::kLostConnection;
      } else if (connect_status == UsbConnectStatus::kInvalidDevice) {
        LOG(ERROR) << "Invalid base connected.";
        dbus_wrapper_->SendSignal(kInvalidBaseConnectedSignal);
      }

      // If there is a "hammer-like" device attached, hammerd should
      // try to avoid running again when hammer jumps to RW. Use kNeedJump
      // to force this wait time before exiting.
      return HammerUpdater::RunStatus::kNeedJump;
    }

    status = RunOnce(post_rw_jump, need_inject_entropy);
    post_rw_jump = false;
    need_inject_entropy = false;
    switch (status) {
      case HammerUpdater::RunStatus::kNoUpdate:
        LOG(INFO) << "Hammer does not need to update.";
        fw_updater_->CloseUsb();
        return status;

      case HammerUpdater::RunStatus::kFatalError:
        LOG(ERROR) << "Hammer encountered a fatal error!";
        // Send the reset signal to hammer, and then prevent the next hammerd
        // process from being invoked.
        fw_updater_->SendSubcommand(UpdateExtraCommand::kImmediateReset);
        fw_updater_->CloseUsb();
        return HammerUpdater::RunStatus::kNeedReset;

      case HammerUpdater::RunStatus::kInvalidFirmware:
        // Send the JumpToRW to hammer, and then prevent the next hammerd
        // process from being invoked.
        fw_updater_->SendSubcommand(UpdateExtraCommand::kJumpToRW);
        fw_updater_->CloseUsb();
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(kResetTimeMs));
        return HammerUpdater::RunStatus::kNeedJump;

      case HammerUpdater::RunStatus::kNeedReset:
        LOG(INFO) << "Reset hammer and run again. run_count=" << run_count;
        fw_updater_->SendSubcommand(UpdateExtraCommand::kImmediateReset);
        fw_updater_->CloseUsb();
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(kResetTimeMs));
        continue;

      case HammerUpdater::RunStatus::kNeedJump:
        post_rw_jump = true;
        LOG(INFO) << "Jump to RW and run again. run_count=" << run_count;
        fw_updater_->SendSubcommand(UpdateExtraCommand::kJumpToRW);
        fw_updater_->CloseUsb();
        // TODO(kitching): Make RW jumps more robust by polling until
        // the jump completes (or fails).
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(kResetTimeMs));
        continue;

      case HammerUpdater::RunStatus::kNeedInjectEntropy:
        need_inject_entropy = true;
        fw_updater_->SendSubcommand(UpdateExtraCommand::kImmediateReset);
        fw_updater_->CloseUsb();
        base::PlatformThread::Sleep(
            base::TimeDelta::FromMilliseconds(kResetTimeMs));
        // If it is the last run, we should treat it as kNeedReset because we
        // just sent a kImmediateReset signal.
        status = HammerUpdater::RunStatus::kNeedReset;
        continue;

      default:
        LOG(ERROR) << "Unknown RunStatus: " << static_cast<int>(status);
        fw_updater_->CloseUsb();
        return HammerUpdater::RunStatus::kFatalError;
    }
  }

  LOG(ERROR) << "Maximum run count exceeded (" << kMaximumRunCount << ")! ";
  return status;
}

HammerUpdater::RunStatus HammerUpdater::RunOnce(
    const bool post_rw_jump, const bool need_inject_entropy) {
  // The first time we use SendFirstPdu it is to gather information about
  // hammer's running EC. We should use SendDone right away to get the EC
  // back into a state where we can send a subcommand.
  if (!fw_updater_->SendFirstPdu()) {
    LOG(ERROR) << "Failed to send the first PDU.";
    return HammerUpdater::RunStatus::kNeedReset;
  }
  fw_updater_->SendDone();

  LOG(INFO) << "### Current Section: "
            << ToString(fw_updater_->CurrentSection()) << " ###";

  // ********************** UNKNOWN **********************
  // If the layout of the firmware has changed, we cannot handle this case.
  if (fw_updater_->CurrentSection() == SectionName::Invalid) {
    LOG(INFO) << "Hammer is in RO but the firmware layout has changed.";
    return HammerUpdater::RunStatus::kInvalidFirmware;
  }

  // ********************** RW **********************
  // If EC already entered the RW section, then check if RW needs updating.
  // If an update is needed, request a hammer reset. Let the next invocation
  // of Run handle the update.
  if (fw_updater_->CurrentSection() == SectionName::RW) {
    if (fw_updater_->VersionMismatch(SectionName::RW)) {
      if (fw_updater_->UpdatePossible(SectionName::RW)) {
        LOG(INFO) << "RW section needs update. Rebooting to RO.";
        NotifyUpdateStarted();
        return HammerUpdater::RunStatus::kNeedReset;
      } else {
        LOG(INFO) << "RW section needs update, but local image is "
                  << "incompatible. Continuing to post-RW process; maybe "
                  << "RO can be updated.";
      }
    }
    return PostRWProcess();
  }

  // ********************** RO **********************
  // Current section is now guaranteed to be RO.  If hammer:
  //   (1) failed to jump to RW after the last run; or
  //   (2) RW section needs updating,
  // then continue with the update procedure.
  if (need_inject_entropy || post_rw_jump ||
      (fw_updater_->VersionMismatch(SectionName::RW) &&
       fw_updater_->UpdatePossible(SectionName::RW))) {
    NotifyUpdateStarted();
    // If we have just finished a jump to RW, but we're still in RO, then
    // we should log the failure.
    if (post_rw_jump) {
      LOG(ERROR) << "Failed to jump to RW. Need to update RW section.";
      if (!fw_updater_->UpdatePossible(SectionName::RW)) {
        LOG(ERROR) << "RW section is unusable, but local image is "
                   << "incompatible. Giving up.";
        return HammerUpdater::RunStatus::kFatalError;
      }
    }

    // EC is still running in RO section. Send "Stay in RO" command before
    // continuing.
    LOG(INFO) << "Sending stay in RO command.";
    if (!fw_updater_->SendSubcommand(UpdateExtraCommand::kStayInRO)) {
      LOG(ERROR) << "Failed to stay in RO.";
      return HammerUpdater::RunStatus::kNeedReset;
    }

    if (need_inject_entropy) {
      bool ret = fw_updater_->InjectEntropy();
      if (ret) {
        LOG(INFO) << "Successfully injected entropy.";
        return HammerUpdater::RunStatus::kNeedReset;
      }
      LOG(ERROR) << "Failed to inject entropy.";
      return HammerUpdater::RunStatus::kFatalError;
    }

    if (fw_updater_->IsSectionLocked(SectionName::RW)) {
      LOG(INFO) << "Unlock RW section, and reset EC.";
      fw_updater_->UnlockSection(SectionName::RW);
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

HammerUpdater::RunStatus HammerUpdater::PostRWProcess() {
  LOG(INFO) << "Start the post-RW process.";
  HammerUpdater::RunStatus ret;

  // Update RO section.
  ret = UpdateRO();
  if (ret != HammerUpdater::RunStatus::kNoUpdate) {
    return ret;
  }

  // Trigger the retry if update fails.
  if (RunTouchpadUpdater() == RunStatus::kTouchpadUpdated) {
    LOG(INFO) << "Touchpad update succeeded.";
  } else {
    LOG(INFO) << "Touchpad update failure.";
    return RunStatus::kNeedReset;
  }

  // Pair with hammer.
  ret = Pair();
  if (ret != HammerUpdater::RunStatus::kNoUpdate) {
    return ret;
  }

  // TODO(akahuang): Rollback increment.
  // All process are done.
  return HammerUpdater::RunStatus::kNoUpdate;
}

HammerUpdater::RunStatus HammerUpdater::UpdateRO() {
  // RO section should be unlocked on dogfood devices -- no need to first run
  // UnLockSection.
  // TODO(kitching): Consider adding a UI warning to make sure a dogfood user
  // does not detach the base at the wrong time, as that could brick it.
  if (fw_updater_->IsSectionLocked(SectionName::RO)) {
    LOG(INFO) << "RO section is locked. Update infeasible.";
    return HammerUpdater::RunStatus::kNoUpdate;
  }
  if (!fw_updater_->VersionMismatch(SectionName::RO)) {
    LOG(INFO) << "RO section is unlocked, but update not needed.";
    return HammerUpdater::RunStatus::kNoUpdate;
  }
  LOG(INFO) << "RO is unlocked and update is needed. Starting update.";
  NotifyUpdateStarted();
  bool ret = fw_updater_->TransferImage(SectionName::RO);
  LOG(INFO) << "RO update " << (ret ? "passed." : "failed.");
  // In the case that the update failed, a reset will either brick the device,
  // or get it back into a normal state.
  return HammerUpdater::RunStatus::kNeedReset;
}

HammerUpdater::RunStatus HammerUpdater::Pair() {
  auto status = pair_manager_->PairChallenge(fw_updater_.get());
  switch (status) {
    case ChallengeStatus::kChallengePassed:
      // TODO(akahuang): Check if the base is swapped.
      return HammerUpdater::RunStatus::kNoUpdate;

    case ChallengeStatus::kNeedInjectEntropy:
      if (fw_updater_->IsRollbackLocked()) {
        if (!fw_updater_->UnlockRollback()) {
          LOG(ERROR) << "Failed to unlock rollback. Skip injecting entropy.";
          return HammerUpdater::RunStatus::kFatalError;
        }
      }
      return HammerUpdater::RunStatus::kNeedInjectEntropy;

    case ChallengeStatus::kChallengeFailed:
      return HammerUpdater::RunStatus::kFatalError;

    case ChallengeStatus::kUnknownError:
      return HammerUpdater::RunStatus::kFatalError;
  }
  return HammerUpdater::RunStatus::kFatalError;
}

void HammerUpdater::WaitUsbReady(HammerUpdater::RunStatus status) {
  // The time period after which hammer automatically jumps to RW section.
  constexpr unsigned int kJumpToRWTimeMs = 1000;
  // The time period from USB device ready to udev invoking hammerd.
  constexpr unsigned int kUdevGuardTimeMs = 1500;

  // If hammerd send reset or jump to RW signal at the last run, hammer will
  // re-connect to the AP and udev will trigger hammerd again. We MUST prohibit
  // the next invocation, otherwise udev will invoke hammerd infinitely.
  //
  // The timing of invocation might be entering into RO section or RW section.
  // Therefore we might wait for USB device once when sending JumpToRW, and wait
  // twice when sending Reset signal.
  if (status == HammerUpdater::RunStatus::kNeedReset ||
      status == HammerUpdater::RunStatus::kNeedJump) {
    LOG(INFO) << "Wait for USB device ready...";
    UsbConnectStatus usb_connection = fw_updater_->TryConnectUsb();
    fw_updater_->CloseUsb();
    // If there is no device there, don't bother waiting.
    if (usb_connection == UsbConnectStatus::kUsbPathEmpty) {
      return;
    }
    if (status == HammerUpdater::RunStatus::kNeedReset) {
      LOG(INFO) << "USB device probably in RO, waiting for it to enter RW.";
      base::PlatformThread::Sleep(
          base::TimeDelta::FromMilliseconds(kJumpToRWTimeMs));

      usb_connection = fw_updater_->TryConnectUsb();
      fw_updater_->CloseUsb();
      // If there is no device there, don't bother waiting.
      if (usb_connection == UsbConnectStatus::kUsbPathEmpty) {
        return;
      }
    }

    LOG(INFO) << "Now USB device should be in RW. Wait "
              << kUdevGuardTimeMs
              << "ms to prevent udev invoking next process.";
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kUdevGuardTimeMs));
    LOG(INFO) << "Finish the infinite loop prevention.";
  }
}

void HammerUpdater::NotifyUpdateStarted() {
  if (!dbus_notified_) {
    dbus_notified_ = true;
    dbus_wrapper_->SendSignal(kBaseFirmwareUpdateStartedSignal);
  }
}

HammerUpdater::RunStatus HammerUpdater::RunTouchpadUpdater() {
  if (!touchpad_image_.size()) {  // We are missing the touchpad file.
    LOG(INFO) << "Touchpad will remain unmodified as binary is not provided.";
    return RunStatus::kTouchpadUpdated;
  }

  LOG(INFO) << "Loading touchpad firmware image.";
  if (!fw_updater_->LoadTouchpadImage(touchpad_image_)) {
    LOG(ERROR) << "Failed to load touchpad image.";
    return RunStatus::kInvalidFirmware;
  }

  // Make request to get infomation from hammer.
  TouchpadInfo response;
  if (!fw_updater_->SendSubcommandReceiveResponse(
          UpdateExtraCommand::kTouchpadInfo, "",
          reinterpret_cast<void*>(&response),
          sizeof(response))) {
      LOG(ERROR) << "Not able to get touchpad info from base.";
      return RunStatus::kNeedReset;
  }
  LOG(INFO) << "status: 0x" << std::hex << static_cast<int>(response.status);
  LOG(INFO) << "vendor: 0x" << std::hex << response.vendor;
  LOG(INFO) << "fw_address: 0x" << std::hex << response.fw_address;
  LOG(INFO) << "fw_size: 0x" << std::hex << response.fw_size;
  LOG(INFO) << "allowed_fw_hash: 0x" <<
      base::HexEncode(response.allowed_fw_hash, SHA256_DIGEST_LENGTH);
  LOG(INFO) << "id: 0x" << std::hex << response.elan.id;
  LOG(INFO) << "fw_version: 0x" << std::hex << response.elan.fw_version;
  LOG(INFO) << "fw_checksum: 0x" << std::hex << response.elan.fw_checksum;

  // Check if the image size matches IC size.
  if (touchpad_image_.size() != response.fw_size) {
    LOG(ERROR) << "Local touchpad binary doesn't match remote IC size.";
    LOG(ERROR) << "Local=" << touchpad_image_.size() << " bytes."
               << "Remote=" << response.fw_size << " bytes.";
    return HammerUpdater::RunStatus::kFatalError;
  }

  // Check if the SHA value of the touchpad firmware (entire file) has same
  // hash as the record in RW firmware. We check this prior to update
  // because if an individual chunk verification fail, the touchpad might
  // get into a weird state (only part of the flash is updated).
  uint8_t digest[SHA256_DIGEST_LENGTH];

  SHA256(reinterpret_cast<const uint8_t*>(touchpad_image_.data()),
         response.fw_size, reinterpret_cast<unsigned char *>(&digest));
  LOG(INFO) << "Computed local touchpad firmware hash: 0x"
            << base::HexEncode(digest, SHA256_DIGEST_LENGTH);
  if (std::memcmp(digest, response.allowed_fw_hash, SHA256_DIGEST_LENGTH)) {
    LOG(ERROR) << "Touchpad firmware mismatches hash in RW EC.";
    return HammerUpdater::RunStatus::kFatalError;
  }

  // TODO(b/65534217): Check if our binary is identical to
  //                   that of hammer by filename.
  if (fw_updater_->TransferTouchpadFirmware(
      response.fw_address, response.fw_size)) {
    return HammerUpdater::RunStatus::kTouchpadUpdated;
  } else {
    return HammerUpdater::RunStatus::kFatalError;
  }
}
}  // namespace hammerd
