// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/usb-device-info.h"

#include <vector>

#include <base/logging.h>
#include <base/string_split.h>
#include <base/string_util.h>

#include "cros-disks/file-reader.h"

using std::map;
using std::string;
using std::vector;

namespace cros_disks {

// A data structure for holding information of a USB device.
struct USBDeviceEntry {
  DeviceMediaType media_type;
};

USBDeviceInfo::USBDeviceInfo() {
}

USBDeviceInfo::~USBDeviceInfo() {
}

DeviceMediaType USBDeviceInfo::GetDeviceMediaType(
    const string& vendor_id, const string& product_id) const {
  CHECK(!vendor_id.empty()) << "Invalid vendor ID";
  CHECK(!product_id.empty()) << "Invalid product ID";

  string id = vendor_id + ":" + product_id;
  map<string, USBDeviceEntry>::const_iterator map_iterator = entries_.find(id);
  if (map_iterator != entries_.end())
    return map_iterator->second.media_type;
  return DEVICE_MEDIA_USB;
}

bool USBDeviceInfo::RetrieveFromFile(const string& path) {
  entries_.clear();

  FileReader reader;
  if (!reader.Open(FilePath(path))) {
    LOG(ERROR) << "Failed to retrieve USB device info from '" << path << "'";
    return false;
  }

  string line;
  while (reader.ReadLine(&line)) {
    // Skip an empty or comment line.
    if (line.empty() || StartsWithASCII(line, "#", true))
      continue;

    vector<string> tokens;
    base::SplitString(line, ' ', &tokens);
    if (tokens.size() >= 2) {
      USBDeviceEntry& entry = entries_[tokens[0]];
      entry.media_type = ConvertToDeviceMediaType(tokens[1]);
    }
  }
  return true;
}

DeviceMediaType USBDeviceInfo::ConvertToDeviceMediaType(
    const string& str) const {
  if (str == "sd") {
    return DEVICE_MEDIA_SD;
  } else if (str == "mobile") {
    return DEVICE_MEDIA_MOBILE;
  } else {
    return DEVICE_MEDIA_USB;
  }
}

}  // namespace cros_disks
