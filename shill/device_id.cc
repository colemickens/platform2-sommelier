// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_id.h"

#include <inttypes.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

namespace shill {

namespace {

// Reads a file containing a string device ID and normalizes it by trimming
// whitespace and converting to lowercase.
bool ReadDeviceIdFile(const base::FilePath& path, std::string* out_id) {
  DCHECK(out_id);
  std::string contents;
  if (!base::ReadFileToString(path, &contents))
    return false;

  *out_id = base::CollapseWhitespaceASCII(base::ToLowerASCII(contents), true);
  return true;
}

bool HextetToUInt16(const std::string& input, uint16_t* output) {
  DCHECK(output);
  std::vector<uint8_t> bytes;
  if (!base::HexStringToBytes(input, &bytes))
    return false;

  if (bytes.size() != 2)
    return false;

  *output = bytes[0] << 8 | bytes[1];
  return true;
}

}  // namespace

std::string DeviceId::AsString() const {
  const char* bus_name;
  switch (bus_type_) {
    case BusType::kUsb:
      bus_name = "usb";
      break;
  }

  if (!has_product_id_) {
    return base::StringPrintf("%s:%04" PRIx16 ":*", bus_name, vendor_id_);
  }

  return base::StringPrintf("%s:%04" PRIx16 ":%04" PRIx16,
                            bus_name,
                            vendor_id_,
                            product_id_);
}

bool DeviceId::Match(const DeviceId& other) const {
  if (bus_type_ != other.bus_type_ || vendor_id_ != other.vendor_id_) {
    return false;
  }

  // If one or both is a VID:* ID, then they don't have to match PID
  // values.
  if (!has_product_id_ || !other.has_product_id_) {
    return true;
  }
  return product_id_ == other.product_id_;
}

std::unique_ptr<DeviceId> ReadDeviceIdFromSysfs(
    const base::FilePath& syspath) {
  if (syspath.empty()) {
    return nullptr;
  }

  base::FilePath subsystem;
  if (!base::ReadSymbolicLink(syspath.Append("subsystem"), &subsystem)) {
    return nullptr;
  }
  std::string bus_type = subsystem.BaseName().value();

  if (bus_type == "usb") {
    std::string vendor_id, product_id;
    uint16_t parsed_vendor_id, parsed_product_id;
    if (!ReadDeviceIdFile(syspath.Append("idVendor"), &vendor_id) ||
        !HextetToUInt16(vendor_id, &parsed_vendor_id) ||
        !ReadDeviceIdFile(syspath.Append("idProduct"), &product_id) ||
        !HextetToUInt16(product_id, &parsed_product_id)) {
      return nullptr;
    }

    return std::make_unique<DeviceId>(
        DeviceId::BusType::kUsb, parsed_vendor_id, parsed_product_id);
  }

  return nullptr;
}

}  // namespace shill
