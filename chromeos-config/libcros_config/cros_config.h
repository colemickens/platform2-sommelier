// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <brillo/brillo_export.h>
#include "chromeos-config/libcros_config/cros_config_interface.h"
#include "chromeos-config/libcros_config/identity.h"

namespace base {
class FilePath;
}  // namespace base

namespace brillo {

static const int kDefaultSkuId = -1;

class BRILLO_EXPORT CrosConfig : public CrosConfigInterface {
 public:
  CrosConfig();
  ~CrosConfig() override;

  // Prepare the configuration system for access to the configuration for
  // the model this is running on. This reads the configuration file into
  // memory.
  // @sku_id: If sku_id is kDefaultSkuId, then the default one normally
  //     returned by 'mosys platform id' would be assigned. Otherwise
  //     (if sku_id is not kDefaultSkuId), the value here will be used
  //     to match the configuration.
  // @return true if OK, false on error.
  bool Init(const int sku_id = kDefaultSkuId);

  // Alias for other clients in platform2 that call InitModel()
  // instead of Init().
  // TODO(jrosenth): remove this alias once nothing is left that calls
  // InitModel().
  // @return true if OK, false on error.
  bool InitModel();

  // Prepare the configuration system for testing.
  // This reads in the given configuration file and selects the config
  // based on the supplied identifiers.
  // @sku_id: SKU ID, normally returned by 'mosys platform sku'.
  // @json_path: Path to configuration file.
  // @arch: brillo::SystemArchitecture (kX86 or kArm).
  // @name: Platform name normally returned by 'mosys platform id'.
  // @customization_id: VPD customization ID
  // @return true if OK, false on error.
  bool InitForTest(const int sku_id,
                   const base::FilePath& json_path,
                   const SystemArchitecture arch,
                   const std::string& name,
                   const std::string& customization_id);

  // CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& property,
                 std::string* val_out) override;

 private:
  // Internal init function used by Init and InitForTest.
  // @sku_id: SKU ID, normally returned by 'mosys platform sku'.
  // @json_path: Path to configuration file.
  // @arch: brillo::SystemArchitecture (kX86 or kArm).
  // @product_name_file: The file to read product name, or device-tree
  //     compatible name, from
  // @product_sku_file: The file to read sku id from
  // @vpd_file: The file to read VPD customization ID from
  // @return true if OK, false on error.
  bool InitInternal(const int sku_id,
                    const base::FilePath& json_path,
                    const SystemArchitecture arch,
                    const base::FilePath& product_name_file,
                    const base::FilePath& product_sku_file,
                    const base::FilePath& vpd_file);

  // Runs a quick init check and prints an error to stderr if it fails.
  // @return true if OK, false on error.
  bool InitCheck() const;

  // The underlying CrosConfigJson used for GetString
  std::unique_ptr<CrosConfigInterface> cros_config_;

  DISALLOW_COPY_AND_ASSIGN(CrosConfig);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
