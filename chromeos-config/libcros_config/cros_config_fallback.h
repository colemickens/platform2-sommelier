// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fallback CrosConfig when running on non-unibuild platforms that
// gets info by calling out to external commands (e.g., mosys)

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_FALLBACK_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_FALLBACK_H_

#include <string>

#include <base/macros.h>
#include <brillo/brillo_export.h>
#include "chromeos-config/libcros_config/cros_config_interface.h"

namespace base {
class FilePath;
}  // namespace base

namespace brillo {

class BRILLO_EXPORT CrosConfigFallback : public CrosConfigInterface {
 public:
  CrosConfigFallback();
  ~CrosConfigFallback() override;

  // Write files corresponding to each defined fallback value into a
  // directory. Each path will correspond to a series of directories,
  // leading up to a single file for the property.
  // @output_dir: Directory to write the files and directories into.
  // @return true on success, false on error.
  bool WriteConfigFS(const base::FilePath& output_dir);

  // CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& property,
                 std::string* val_out) override;

  bool GetDeviceIndex(int* device_index_out) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosConfigFallback);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_FALLBACK_H_
