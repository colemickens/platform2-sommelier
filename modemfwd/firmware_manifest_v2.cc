// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/firmware_manifest.h"

#include <utility>

#include <base/strings/string_number_conversions.h>
#include <brillo/proto_file_io.h>

#include "modemfwd/firmware_directory.h"

#include "modemfwd/proto_bindings/firmware_manifest_v2.pb.h"

namespace modemfwd {

namespace {

bool ParseDevice(const Device& device,
                 const base::FilePath& directory_path,
                 DeviceFirmwareCache* out_cache) {
  // Sort main firmware entries by version. Ensure the versions are
  // all separate.
  std::map<std::string, std::unique_ptr<FirmwareFileInfo>> main_firmware_infos;
  for (const MainFirmwareV2& main_firmware : device.main_firmware()) {
    if (main_firmware.filename().empty() || main_firmware.version().empty() ||
        !Compression_IsValid(main_firmware.compression())) {
      LOG(ERROR) << "Found malformed main firmware manifest entry";
      return false;
    }
    if (main_firmware_infos.count(main_firmware.version()) > 0) {
      LOG(ERROR) << "Found multiple main firmware with the same version for "
                 << "device " << device.device_id()
                 << (device.variant().empty()
                         ? ""
                         : "(variant " + device.variant() + ")");
      return false;
    }

    auto compression =
        ToFirmwareFileInfoCompression(main_firmware.compression());
    if (!compression.has_value())
      return false;

    main_firmware_infos.emplace(
        main_firmware.version(),
        std::make_unique<FirmwareFileInfo>(
            directory_path.Append(main_firmware.filename()),
            main_firmware.version(), compression.value()));
  }

  // Main firmware is default for a device if:
  // * It is explicitly specified as default in the Device entry.
  // * It is the only main firmware.
  FirmwareFileInfo* default_main_entry = nullptr;
  if (!device.default_main_firmware_version().empty()) {
    if (main_firmware_infos.count(device.default_main_firmware_version()) ==
        0) {
      LOG(ERROR) << "Firmware manifest specified invalid default main firmware "
                    "version";
      return false;
    }

    default_main_entry =
        main_firmware_infos[device.default_main_firmware_version()].get();
  } else if (main_firmware_infos.size() == 1) {
    default_main_entry = main_firmware_infos.begin()->second.get();
  }

  // If not, then each carrier firmware must specify a functional main firmware
  // version, and there must be a generic carrier firmware supplying the main
  // version if no explicitly supported carrier is found.

  for (const CarrierFirmwareV2& carrier_firmware : device.carrier_firmware()) {
    if (carrier_firmware.filename().empty() ||
        carrier_firmware.version().empty() ||
        carrier_firmware.carrier_id().empty() ||
        !Compression_IsValid(carrier_firmware.compression())) {
      LOG(ERROR) << "Found malformed carrier firmware manifest entry";
      return false;
    }

    // Convert the manifest entry into a FirmwareFileInfo.
    auto compression =
        ToFirmwareFileInfoCompression(carrier_firmware.compression());
    if (!compression.has_value())
      return false;

    // There must either be a default main firmware or an explicitly specified
    // one here.
    FirmwareFileInfo* main_firmware_for_carrier;
    if (!carrier_firmware.main_firmware_version().empty()) {
      if (main_firmware_infos.count(carrier_firmware.main_firmware_version()) ==
          0) {
        LOG(ERROR)
            << "Manifest specified invalid default main firmware version";
        return false;
      }
      main_firmware_for_carrier =
          main_firmware_infos[carrier_firmware.main_firmware_version()].get();
    } else if (default_main_entry) {
      main_firmware_for_carrier = default_main_entry;
    } else {
      LOG(ERROR) << "No main firmware specified for carrier firmware "
                 << carrier_firmware.filename();
      return false;
    }

    auto carrier_info = std::make_unique<FirmwareFileInfo>(
        directory_path.Append(carrier_firmware.filename()),
        carrier_firmware.version(), compression.value());

    // Add the firmware to the cache under the carrier ID for this entry.
    for (const std::string& supported_carrier : carrier_firmware.carrier_id()) {
      if (out_cache->carrier_firmware.count(supported_carrier) > 0) {
        LOG(ERROR) << "Duplicate carrier firmware entry for carrier "
                   << supported_carrier;
        // We haven't inserted main firmware into the mapping yet. Clear all
        // the indices to prevent poorly-behaved users from getting dangling
        // pointers.
        out_cache->main_firmware.clear();
        out_cache->carrier_firmware.clear();
        return false;
      }

      out_cache->main_firmware[supported_carrier] = main_firmware_for_carrier;
      out_cache->carrier_firmware[supported_carrier] = carrier_info.get();
    }
    out_cache->all_files.push_back(std::move(carrier_info));
  }

  // Now it's safe to move all of the main firmware file info pointers.
  for (auto& main_info : main_firmware_infos)
    out_cache->all_files.push_back(std::move(main_info.second));

  // If we have a default entry but didn't see any generic carrier firmware,
  // we put the default main firmware in the main index under generic.
  if (out_cache->main_firmware.count(FirmwareDirectory::kGenericCarrierId) ==
      0) {
    if (!default_main_entry) {
      LOG(ERROR) << "Manifest did not supply generic main firmware";
      return false;
    }
    out_cache->main_firmware[FirmwareDirectory::kGenericCarrierId] =
        default_main_entry;
  }

  return true;
}

}  // namespace

bool ParseFirmwareManifestV2(const base::FilePath& manifest,
                             FirmwareIndex* index) {
  FirmwareManifestV2 manifest_proto;
  if (!brillo::ReadTextProtobuf(manifest, &manifest_proto))
    return false;

  base::FilePath directory = manifest.DirName();

  for (const Device& device : manifest_proto.device()) {
    if (device.device_id().empty()) {
      LOG(ERROR) << "Empty device ID in device entry";
      return false;
    }

    DeviceType type{device.device_id(), device.variant()};
    if (index->count(type) > 0) {
      LOG(ERROR) << "Duplicate device entry in manifest";
      return false;
    }

    DeviceFirmwareCache cache;
    if (!ParseDevice(device, directory, &cache))
      return false;

    (*index)[type] = std::move(cache);
  }

  return true;
}

}  // namespace modemfwd
