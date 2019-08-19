// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fallback CrosConfig when running on non-unibuild platforms that
// gets info by calling out to external commands (e.g., mosys)

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_FALLBACK_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_FALLBACK_H_

#include <string>

#include <base/macros.h>
#include "chromeos-config/libcros_config/cros_config_interface.h"

namespace brillo {

class CrosConfigFallback : public CrosConfigInterface {
 public:
  CrosConfigFallback();
  ~CrosConfigFallback() override;

  // CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& property,
                 std::string* val_out) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrosConfigFallback);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_FALLBACK_H_
