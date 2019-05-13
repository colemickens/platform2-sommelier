// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/firmware_directory.h"

#include <memory>
#include <utility>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>
#include <brillo/proto_file_io.h>
#include <cros_config/cros_config.h>

#include "modemfwd/proto_bindings/firmware_manifest.pb.h"

namespace modemfwd {

namespace {

const char kManifestName[] = "firmware_manifest.prototxt";

// Returns the modem firmware variant for the current model of the device by
// reading the /modem/firmware-variant property of the current model via
// chromeos-config. Returns an empty string if it fails to read the modem
// firmware variant from chromeos-config or no modem firmware variant is
// specified.
std::string GetModemFirmwareVariant() {
  brillo::CrosConfig config;
  if (!config.InitModel()) {
    LOG(WARNING) << "Failed to load Chrome OS configuration";
    return std::string();
  }

  std::string variant;
  if (!config.GetString("/modem", "firmware-variant", &variant)) {
    LOG(INFO) << "No modem firmware variant is specified";
    return std::string();
  }

  LOG(INFO) << "Use modem firmware variant: " << variant;
  return variant;
}

FirmwareFileInfo::Compression ToFirmwareFileInfoCompression(
    Compression compression) {
  switch (compression) {
    case Compression::NONE:
      return FirmwareFileInfo::Compression::NONE;
    case Compression::XZ:
      return FirmwareFileInfo::Compression::XZ;
    default:
      LOG(FATAL) << "Invalid compression: " << compression;
      return FirmwareFileInfo::Compression::NONE;
  }
}

}  // namespace

const char FirmwareDirectory::kGenericCarrierId[] = "generic";

class FirmwareDirectoryImpl : public FirmwareDirectory {
 public:
  FirmwareDirectoryImpl(const FirmwareManifest& manifest,
                        const base::FilePath& directory)
      : manifest_(manifest),
        directory_(directory),
        variant_(GetModemFirmwareVariant()) {}

  // modemfwd::FirmwareDirectory overrides.
  bool FindMainFirmware(const std::string& device_id,
                        FirmwareFileInfo* out_info) override {
    DCHECK(out_info);
    for (const MainFirmware& file_info : manifest_.main_firmware()) {
      if (file_info.device_id() == device_id &&
          (file_info.variant().empty() || file_info.variant() == variant_) &&
          !file_info.filename().empty() && !file_info.version().empty()) {
        out_info->firmware_path = directory_.Append(file_info.filename());
        out_info->version = file_info.version();
        out_info->compression =
            ToFirmwareFileInfoCompression(file_info.compression());
        return true;
      }
    }

    return false;
  }

  bool FindCarrierFirmware(const std::string& device_id,
                           std::string* carrier_id,
                           FirmwareFileInfo* out_info) override {
    DCHECK(carrier_id);
    if (FindSpecificCarrierFirmware(device_id, *carrier_id, out_info))
      return true;

    if (FindSpecificCarrierFirmware(device_id, kGenericCarrierId, out_info)) {
      *carrier_id = kGenericCarrierId;
      return true;
    }

    return false;
  }

 private:
  bool FindSpecificCarrierFirmware(const std::string& device_id,
                                   const std::string& carrier_id,
                                   FirmwareFileInfo* out_info) {
    DCHECK(out_info);
    for (const CarrierFirmware& file_info : manifest_.carrier_firmware()) {
      if (file_info.device_id() != device_id ||
          (!file_info.variant().empty() && file_info.variant() != variant_) ||
          file_info.filename().empty() || file_info.version().empty()) {
        continue;
      }

      for (const std::string& supported_carrier : file_info.carrier_id()) {
        if (supported_carrier == carrier_id) {
          out_info->firmware_path = directory_.Append(file_info.filename());
          out_info->version = file_info.version();
          out_info->compression =
              ToFirmwareFileInfoCompression(file_info.compression());
          return true;
        }
      }
    }

    return false;
  }

  FirmwareManifest manifest_;
  base::FilePath directory_;
  std::string variant_;

  DISALLOW_COPY_AND_ASSIGN(FirmwareDirectoryImpl);
};

std::unique_ptr<FirmwareDirectory> CreateFirmwareDirectory(
    const base::FilePath& directory) {
  FirmwareManifest parsed_manifest;
  if (!brillo::ReadTextProtobuf(directory.Append(kManifestName),
                                &parsed_manifest))
    return nullptr;

  return std::make_unique<FirmwareDirectoryImpl>(parsed_manifest, directory);
}

}  // namespace modemfwd
