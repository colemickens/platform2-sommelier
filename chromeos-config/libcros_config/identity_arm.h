// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reads ARM identity information and checks for device compatibility.

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_ARM_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_ARM_H_

#include "chromeos-config/libcros_config/identity.h"

#include <string>

#include <base/files/file_path.h>

namespace brillo {

class CrosConfigIdentityArm : public CrosConfigIdentity {
 public:
  CrosConfigIdentityArm();
  ~CrosConfigIdentityArm();

  // Read the compatible devices list from the device-tree compatible file.
  //
  // @dt_compatible_file: File to read - typically /proc/device-tree/compatible
  bool ReadDtCompatible(const base::FilePath& dt_compatible_file);

  // Write out fake device-tree compatible file for testing purposes.
  // @device_name: Device name to write to the compatible file
  // @dt_compatible_file_out: Returns the file that was written
  // @return true if OK, false on error
  bool FakeDtCompatible(const std::string& device_name,
                        base::FilePath* dt_compatible_file_out);

  // Checks if the device_name exists in the compatible devices string.
  // @return true if device is compatible
  bool IsCompatible(const std::string& device_name) const;

  // @return Compatible devices string read via ReadDtCompatible
  const std::string& GetCompatibleDeviceString() const {
    return compatible_devices_;
  }

 private:
  std::string compatible_devices_;

  DISALLOW_COPY_AND_ASSIGN(CrosConfigIdentityArm);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_ARM_H_
