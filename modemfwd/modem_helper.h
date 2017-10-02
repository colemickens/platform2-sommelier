// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_MODEM_HELPER_H_
#define MODEMFWD_MODEM_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>

namespace modemfwd {

struct CarrierFirmwareInfo {
  CarrierFirmwareInfo() = default;
  CarrierFirmwareInfo(const std::string& carrier_name,
                      const std::string& version)
      : carrier_name(carrier_name), version(version) {}

  std::string carrier_name;
  std::string version;
};

class ModemHelper {
 public:
  virtual ~ModemHelper() = default;

  virtual bool FlashMainFirmware(const base::FilePath& path_to_fw) = 0;

  virtual bool GetCarrierFirmwareInfo(CarrierFirmwareInfo* out_info) = 0;
  virtual bool FlashCarrierFirmware(const base::FilePath& path_to_fw) = 0;
};

std::unique_ptr<ModemHelper> CreateModemHelper(
    const base::FilePath& helper_path);

}  // namespace modemfwd

#endif  // MODEMFWD_MODEM_HELPER_H_
