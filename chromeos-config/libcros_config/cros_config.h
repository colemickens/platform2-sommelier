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
#include "chromeos-config/libcros_config/cros_config_impl.h"
#include "chromeos-config/libcros_config/cros_config_interface.h"

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
  // @return true if OK, false on error.
  bool InitModel();
  // If the different configuration other then the model this device running on
  // is intended to check then the explicit sku_id can be assigned.
  // @sku_id: If sku_id is kDefaultSkuId  then the default one returned by
  //     'mosys platform id' would be assigned. Or the value here will be used
  //     to match the configuration.
  // @return true if OK, false on error.
  bool InitModel(const int sku_id);

  // Alias for the above, since this is used by several clients.
  bool Init();
  bool Init(const int sku_id);

  // Prepare the configuration system for testing on X86 devices.
  // This reads in the given configuration file and selects the config
  // based on the supplied X86 identifiers.
  // @filepath: Path to configuration .dtb|.json file.
  // @name: Platform name as returned by 'mosys platform id'.
  // @sku_id: SKU ID as returned by 'mosys platform sku'.
  // @customization_id: VPD customization ID
  // @return true if OK, false on error.
  bool InitForTestX86(const base::FilePath& filepath,
                      const std::string& name,
                      int sku_id,
                      const std::string& customization_id);

  // Prepare the configuration system for testing on ARM devices.
  // This reads in the given configuration file and selects the config
  // based on the supplied ARM identifiers.
  // @filepath: Path to configuration .dtb|.json file.
  // @dt_compatible_name: ARM device-tree compatible name string.
  // @sku_id: SKU ID as returned by 'mosys platform sku'.
  // @customization_id: VPD customization ID
  // @return true if OK, false on error.
  bool InitForTestArm(const base::FilePath& filepath,
                      const std::string& dt_compatible_name,
                      int sku_id,
                      const std::string& customization_id);

  // CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& property,
                 std::string* val_out) override;

 private:
  // Common initialization function based on x86 identity.
  // @product_name_file: File containing the product name.
  // @product_sku_file: File containing the SKU string.
  // @vpd_file: File containing the customization_id from VPD. Typically this
  //     is '/sys/firmware/vpd/ro/customization_id'.
  // @sku_id:If value is kDefaultSkuId then identity will extract sku_id from
  //     smbios_file which is the same with `mosys platform id`. Or this value
  //     will be set into identity to replace the default one from smbios.
  //   results for every query
  bool SelectConfigByIdentityX86(
      const base::FilePath& product_name_file,
      const base::FilePath& product_sku_file,
      const base::FilePath& vpd_file,
      const int sku_id = kDefaultSkuId);

  // Common initialization function based on ARM identity.
  // @dt_compatible_file: ARM based device-tree compatible file
  // @sku_id_file: File containing the SKU interger.
  // @vpd_file: File containing the customization_id from VPD. Typically this
  //     is '/sys/firmware/vpd/ro/customization_id'.
  // @sku_id:If value is kDefaultSkuId then identity will get sku_id from
  //     FDT which is the same with `mosys platform id`. Or this value
  //     will be set into identity to replace the default one from FDT.
  //   results for every query
  bool SelectConfigByIdentityArm(const base::FilePath& dt_compatible_file,
                                 const base::FilePath& sku_id_file,
                                 const base::FilePath& vpd_file,
                                 const int sku_id = kDefaultSkuId);

  // Creates the appropriate instance of CrosConfigImpl based on the underlying
  // file type (.dtb|.json).
  // @filepath: path to configuration .dtb|.json file.
  bool InitCrosConfig(const base::FilePath& filepath);

  std::unique_ptr<CrosConfigImpl> cros_config_;

  // Runs a quick init check and prints an error to stderr if it fails.
  // @return true if OK, false on error.
  bool InitCheck() const;

  DISALLOW_COPY_AND_ASSIGN(CrosConfig);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
