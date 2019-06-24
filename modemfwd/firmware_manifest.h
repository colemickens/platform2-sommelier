// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_FIRMWARE_MANIFEST_H_
#define MODEMFWD_FIRMWARE_MANIFEST_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/optional.h>

#include "modemfwd/firmware_file_info.h"

#include "modemfwd/proto_bindings/firmware_manifest.pb.h"

namespace modemfwd {

base::Optional<FirmwareFileInfo::Compression> ToFirmwareFileInfoCompression(
    Compression compression);

struct DeviceType {
  explicit DeviceType(const std::string& device_id) : device_id_(device_id) {}
  DeviceType(const std::string& device_id, const std::string& variant)
      : device_id_(device_id), variant_(variant) {}

  bool operator<(const DeviceType& other) const {
    return std::tie(device_id_, variant_) <
           std::tie(other.device_id_, other.variant_);
  }

  const std::string& device_id() const { return device_id_; }
  const std::string& variant() const { return variant_; }

 private:
  std::string device_id_;
  std::string variant_;
};

struct DeviceFirmwareCache {
  using CarrierIndex = std::map<std::string, FirmwareFileInfo*>;

  std::vector<std::unique_ptr<FirmwareFileInfo>> all_files;
  CarrierIndex main_firmware;
  CarrierIndex carrier_firmware;
};

using FirmwareIndex = std::map<DeviceType, DeviceFirmwareCache>;

bool ParseFirmwareManifest(const base::FilePath& manifest,
                           FirmwareIndex* index);

bool ParseFirmwareManifestV2(const base::FilePath& manifest,
                             FirmwareIndex* index);

}  // namespace modemfwd

#endif  // MODEMFWD_FIRMWARE_MANIFEST_H_
