// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/firmware_directory.h"

#include <memory>
#include <utility>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "modemfwd/proto_bindings/firmware_manifest.pb.h"
#include "modemfwd/proto_file_io.h"

namespace {

const char kManifestName[] = "firmware_manifest.prototxt";

}  // namespace

namespace modemfwd {

const char FirmwareDirectory::kGenericCarrierId[] = "generic";

class FirmwareDirectoryImpl : public FirmwareDirectory {
 public:
  explicit FirmwareDirectoryImpl(const FirmwareManifest& manifest,
                                 const base::FilePath& directory)
      : manifest_(manifest), directory_(directory) {}

  // modemfwd::FirmwareDirectory overrides.
  bool FindMainFirmware(const std::string& device_id,
                        FirmwareFileInfo* out_info) override {
    DCHECK(out_info);
    for (const MainFirmware& file_info : manifest_.main_firmware()) {
      if (file_info.device_id() == device_id && !file_info.filename().empty() &&
          !file_info.version().empty()) {
        out_info->firmware_path = directory_.Append(file_info.filename());
        out_info->version = file_info.version();
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
      if (file_info.device_id() != device_id || file_info.filename().empty() ||
          file_info.version().empty()) {
        continue;
      }

      for (const std::string& supported_carrier : file_info.carrier_id()) {
        if (supported_carrier == carrier_id) {
          out_info->firmware_path = directory_.Append(file_info.filename());
          out_info->version = file_info.version();
          return true;
        }
      }
    }

    return false;
  }

  FirmwareManifest manifest_;
  base::FilePath directory_;

  DISALLOW_COPY_AND_ASSIGN(FirmwareDirectoryImpl);
};

std::unique_ptr<FirmwareDirectory> CreateFirmwareDirectory(
    const base::FilePath& directory) {
  FirmwareManifest parsed_manifest;
  if (!ReadProtobuf(directory.Append(kManifestName), &parsed_manifest))
    return nullptr;

  return std::make_unique<FirmwareDirectoryImpl>(parsed_manifest, directory);
}

}  // namespace modemfwd
