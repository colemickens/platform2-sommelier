// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_

#include "chromeos-config/libcros_config/cros_config_impl.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/files/file.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/values.h>
#include <brillo/brillo_export.h>

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace brillo {

class BRILLO_EXPORT CrosConfig : public CrosConfigInterface {
 public:
  CrosConfig();
  ~CrosConfig() override;

  // Prepare the configuration system for for access to the configuration for
  // the model this is running on. This reads the configuration file into
  // memory.
  // @return true if OK, false on error.
  bool InitModel();

  // Alias for the above, since this is used by several clients.
  bool Init();

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
  // @customization_id: VPD customization ID
  // @return true if OK, false on error.
  bool InitForTestArm(const base::FilePath& filepath,
                      const std::string& dt_compatible_name,
                      const std::string& customization_id);

  // Internal function to obtain a property value and return a list of log
  // messages on failure. Public for tests.
  // @path: Path to locate. Must start with "/".
  // @prop: Property name to look up
  // @val_out: returns the string value found, if any
  // @log_msgs_out: returns a list of error messages if this function fails
  // @return true if found, false if not found
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out,
                 std::vector<std::string>* log_msgs_out);

  // CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out) override;

  // CrosConfigInterface:
  bool GetAbsPath(const std::string& path,
                  const std::string& prop,
                  std::string* val_out) override;

 private:
  // Common initialization function based on x86 identity.
  // @product_name_file: File containing the product name.
  // @product_sku_file: File containing the SKU string.
  // @vpd_file: File containing the customization_id from VPD. Typically this
  //     is '/sys/firmware/vpd/ro/customization_id'.
  //   results for every query
  bool SelectConfigByIdentityX86(
      const base::FilePath& product_name_file,
      const base::FilePath& product_sku_file,
      const base::FilePath& vpd_file);

  // Common initialization function based on ARM identity.
  // @dt_compatible_file: ARM based device-tree compatible file
  // @vpd_file: File containing the customization_id from VPD. Typically this
  //     is '/sys/firmware/vpd/ro/customization_id'.
  //   results for every query
  bool SelectConfigByIdentityArm(const base::FilePath& dt_compatible_file,
                                 const base::FilePath& vpd_file);

  // Creates the appropriate instance of CrosConfigImpl based on the underlying
  // file type (.dtb|.json).
  // @filepath: path to configuration .dtb|.json file.
  bool InitCrosConfig(const base::FilePath& filepath);

  std::unique_ptr<CrosConfigImpl> cros_config_;

 private:
  // Runs a quick init check and prints an error to stderr if it fails.
  // @return true if OK, false on error.
  bool InitCheck() const;

  DISALLOW_COPY_AND_ASSIGN(CrosConfig);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_H_
