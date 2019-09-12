// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_X86_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_X86_H_

#include <string>

#include <base/macros.h>
#include "chromeos-config/libcros_config/identity.h"

namespace base {
class FilePath;
}  // namespace base

namespace brillo {

class CrosConfigIdentityX86 : public CrosConfigIdentity {
 public:
  CrosConfigIdentityX86();
  ~CrosConfigIdentityX86();

  // @return Name value read via ReadSmbios
  const std::string& GetName() const { return name_; }

  // @return SKU ID value read via ReadSmbios
  int GetSkuId() const { return sku_id_; }

  // Initially, the SKU ID will be read from smbios but if user would like to
  // have the identify with different SKU ID then you can overwrite it here.
  void SetSkuId(const int sku_id) { sku_id_ = sku_id; }

  // Read the device identity infromation from the kernel files.
  // This information is set up by AP firmware, so in effect AP firmware
  // sets the device identity.
  //
  // @product_name_file: File containing product name
  // @product_sku_file: File containing SKU ID string
  bool ReadInfo(const base::FilePath& product_name_file,
                const base::FilePath& product_sku_file);

  // Write out fake file containing fake identity information.
  // This is only used for testing.
  // @name: Platform name to write (e.g. 'Reef')
  // @sku_id: SKU ID number to write (e.g. 8)
  // @product_name_file_out: File that the name was written into
  // @product_sku_file_out: File that the SKU ID string was written into
  // @return true if OK, false on error
  bool Fake(const std::string& name,
            int sku_id,
            base::FilePath* product_name_file_out,
            base::FilePath* product_sku_file_out);

 private:
  std::string name_;
  int sku_id_;

  DISALLOW_COPY_AND_ASSIGN(CrosConfigIdentityX86);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_IDENTITY_X86_H_
