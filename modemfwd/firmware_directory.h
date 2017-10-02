// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_FIRMWARE_DIRECTORY_H_
#define MODEMFWD_FIRMWARE_DIRECTORY_H_

#include <memory>
#include <string>

#include <base/files/file_path.h>

namespace modemfwd {

struct FirmwareFileInfo {
  FirmwareFileInfo() = default;
  FirmwareFileInfo(const base::FilePath& firmware_path,
                   const std::string& version)
      : firmware_path(firmware_path), version(version) {}

  base::FilePath firmware_path;
  std::string version;
};

class FirmwareDirectory {
 public:
  static const char kGenericCarrierId[];

  virtual ~FirmwareDirectory() = default;

  // Finds main firmware in the firmware directory for modems with device ID
  // |device_id|. Returns true and sets fields of |out_info| on success and
  // false otherwise.
  virtual bool FindMainFirmware(const std::string& device_id,
                                FirmwareFileInfo* out_info) = 0;

  // Finds carrier firmware in the firmware directory for modems with device ID
  // |device_id| for the carrier |carrier_name|. Returns true and sets fields of
  // |out_info| on success and false otherwise.
  //
  // |carrier_id| may be changed if we find a different carrier firmware
  // that supports the carrier |carrier_id|, such as a generic one.
  virtual bool FindCarrierFirmware(const std::string& device_id,
                                   std::string* carrier_id,
                                   FirmwareFileInfo* out_info) = 0;
};

std::unique_ptr<FirmwareDirectory> CreateFirmwareDirectory(
    const base::FilePath& directory);

}  // namespace modemfwd

#endif  // MODEMFWD_FIRMWARE_DIRECTORY_H_
