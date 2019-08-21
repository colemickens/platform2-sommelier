// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reads ARM identity information and checks for device compatibility.

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_ARM_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_ARM_H_

#include <string>

#include "chromeos-config/libcros_config/identity.h"

namespace base {
class FilePath;
}  // namespace base

namespace brillo {

class CrosConfigIdentityArm : public CrosConfigIdentity {
 public:
  CrosConfigIdentityArm();
  ~CrosConfigIdentityArm();

  // @return SKU ID value read via FDT
  int GetSkuId() const { return sku_id_; }

  // Initially, the SKU ID will be read from FDT but if user would like to
  // have the identify with different SKU ID then you can overwrite it here.
  void SetSkuId(const int sku_id) { sku_id_ = sku_id; }

  // Read the compatible devices list from the device-tree compatible file.
  //
  // @dt_compatible_file: File to read - typically /proc/device-tree/compatible
  // @sku_id_file: File containing SKU ID integer
  bool ReadInfo(const base::FilePath& dt_compatible_file,
                const base::FilePath& sku_id_file);

  // Write out fake device-tree compatible file for testing purposes.
  // @device_name: Device name to write to the compatible file
  // @sku_id_file: File containing SKU ID integer
  // @dt_compatible_file_out: Returns the file that was written
  // @sku_id_file_out: File that the SKU ID integer was written into
  // @return true if OK, false on error
  bool Fake(const std::string& device_name,
            int sku_id,
            base::FilePath* dt_compatible_file_out,
            base::FilePath* sku_id_file_out);

  // Checks if the device_name exists in the compatible devices string.
  // @return true if device is compatible
  bool IsCompatible(const std::string& device_name) const;

  // @return Compatible devices string read via ReadDtCompatible
  const std::string& GetCompatibleDeviceString() const {
    return compatible_devices_;
  }

 private:
  std::string compatible_devices_;
  int sku_id_;

  DISALLOW_COPY_AND_ASSIGN(CrosConfigIdentityArm);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_ARM_H_
