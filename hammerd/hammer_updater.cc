// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// hammerd - A daemon to update the firmware of Hammer

#include "hammerd/hammer_updater.h"

#include <unistd.h>

#include <base/logging.h>

#include "hammerd/update_fw.h"

namespace hammerd {

HammerUpdater::HammerUpdater(const std::string& image) : image_(image) {}

bool HammerUpdater::Run() {
  LOG(INFO) << "Load and validate the image.";
  if (!fw_updater_.LoadImage(image_)) {
    LOG(ERROR) << "Failed to load image.";
    return false;
  }

  // TODO(akahuang): Send reboot command before it?
  if (!fw_updater_.SendSubcommand(UpdateExtraCommand::kStayInRO)) {
    LOG(ERROR) << "Failed to stay in RO";
    return false;
  }

  LOG(INFO) << "Try to update RW firmware.";
  // TODO(akahuang): Notify Chrome UI before updating firmware.
  int num = fw_updater_.TransferImage("RW");
  if (num == -1) {
    LOG(ERROR) << "Failed to update RW firmware";
    return false;
  }
  if (num > 0) {
    LOG(INFO) << "Successfully update the RW firmware.";
    return true;
  }

  LOG(INFO) << "No need to update RW firmware.";
  fw_updater_.SendSubcommand(UpdateExtraCommand::kJumpToRW);
  // TODO(akahuang): Use write protect status of RO section to determine.
  bool dogfood_mode = false;
  if (dogfood_mode) {
    // TODO(akahuang): Update RO firmware.
  }
  // TODO(akahuang): Update trackpad FW.
  // TDOO(akahuang): Pairing.
  // TODO(akahuang): Rollback increment.
  return true;
}

}  // namespace hammerd
