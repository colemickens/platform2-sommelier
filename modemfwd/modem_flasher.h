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
  std::unique_ptr<FirmwareDirectory> firmware_directory_;
  std::unique_ptr<Journal> journal_;

  // Unlike carrier firmware, we should only ever successfully flash the main
  // firmware at most once per boot. In the past vendors have failed to
  // update the version that the firmware reports itself as so we can mitigate
  // some of the potential issues by recording which modems we have deemed
  // don't need updates or were already updated and avoid checking them again.
  std::set<std::string> main_fw_checked_;

  // For carrier firmware, once we've tried to upgrade versions on a particular
  // modem without changing carriers, we should not try to upgrade versions
  // again (but should still flash if the carrier is different) to avoid the
  // same problem as the above. Keep track of the last carrier firmware
  // we flashed so we don't flash twice in a row.
  std::map<std::string, base::FilePath> last_carrier_fw_flashed_;

  // If we fail to flash firmware, we should blacklist the modem so we don't
  // keep trying over and over when the modem reboots and comes back.
  std::set<std::string> blacklist_;

  DISALLOW_COPY_AND_ASSIGN(ModemFlasher);
};

}  // namespace modemfwd

#endif  // MODEMFWD_MODEM_FLASHER_H_
