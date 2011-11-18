// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_USB_DEVICE_INFO_H_
#define CROS_DISKS_USB_DEVICE_INFO_H_

#include <string>
#include <map>

#include <base/basictypes.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest_prod.h>

namespace cros_disks {

struct USBDeviceEntry;

// A class for querying information from a USB device info file.
class USBDeviceInfo {
 public:
  USBDeviceInfo();
  ~USBDeviceInfo();

  // Returns the device media type of a USB device with |vendor_id| and
  // |product_id|.
  DeviceMediaType GetDeviceMediaType(const std::string& vendor_id,
                                     const std::string& product_id) const;

  // Retrieves the list of USB device info from a file at |path|.
  // Returns true on success.
  bool RetrieveFromFile(const std::string& path);

 private:
  // Converts from string to enum of a device media type.
  DeviceMediaType ConvertToDeviceMediaType(const std::string& str) const;

  // A map from an ID string, in form of <vendor id>:<product id>, to a
  // USBDeviceEntry struct.
  std::map<std::string, USBDeviceEntry> entries_;

  FRIEND_TEST(USBDeviceInfoTest, ConvertToDeviceMediaType);

  DISALLOW_COPY_AND_ASSIGN(USBDeviceInfo);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_USB_DEVICE_INFO_H_
