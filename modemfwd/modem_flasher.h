// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_MODEM_FLASHER_H_
#define MODEMFWD_MODEM_FLASHER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include <base/callback.h>
#include <base/macros.h>

#include "modemfwd/firmware_directory.h"
#include "modemfwd/journal.h"
#include "modemfwd/modem.h"

namespace modemfwd {

// ModemFlasher contains all of the logic to make decisions about whether
// or not it should flash new firmware onto the modem.
class ModemFlasher {
 public:
  ModemFlasher(std::unique_ptr<FirmwareDirectory> firmware_directory,
               std::unique_ptr<Journal> journal);

  // Returns a callback that should be executed when the modem reappears.
  base::Closure TryFlash(Modem* modem);

 private:
  class FlashState {
   public:
    FlashState() : main_fw_checked_(false), tries_(2) {}
    ~FlashState() = default;

    void OnFlashFailed() { tries_--; }
    bool ShouldFlash() const { return tries_ > 0; }

    void OnFlashedMainFirmware() { main_fw_checked_ = true; }
    bool ShouldFlashMainFirmware() const { return !main_fw_checked_; }

    void OnFlashedCarrierFirmware(const base::FilePath& path) {
      last_carrier_fw_flashed_ = path;
    }
    bool ShouldFlashCarrierFirmware(const base::FilePath& path) const {
      return last_carrier_fw_flashed_ != path;
    }

   private:
    // Unlike carrier firmware, we should only ever successfully flash the main
    // firmware at most once per boot. In the past vendors have failed to
    // update the version that the firmware reports itself as so we can mitigate
    // some of the potential issues by recording which modems we have deemed
    // don't need updates or were already updated and avoid checking them again.
    bool main_fw_checked_;

    // For carrier firmware, once we've tried to upgrade versions on a
    // particular modem without changing carriers, we should not try to upgrade
    // versions again (but should still flash if the carrier is different) to
    // avoid the same problem as the above. Keep track of the last carrier
    // firmware we flashed so we don't flash twice in a row.
    base::FilePath last_carrier_fw_flashed_;

    // If we fail to flash firmware, we will retry once, but after that we
    // should stop flashing the modem to prevent us from trying it over and
    // over.
    int tries_;
  };

  std::unique_ptr<FirmwareDirectory> firmware_directory_;
  std::unique_ptr<Journal> journal_;

  std::map<std::string, FlashState> modem_info_;

  DISALLOW_COPY_AND_ASSIGN(ModemFlasher);
};

}  // namespace modemfwd

#endif  // MODEMFWD_MODEM_FLASHER_H_
