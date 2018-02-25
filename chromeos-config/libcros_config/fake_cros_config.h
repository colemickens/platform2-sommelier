// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_FAKE_CROS_CONFIG_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_FAKE_CROS_CONFIG_H_

#include "chromeos-config/libcros_config/cros_config_interface.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/macros.h>
#include <brillo/brillo_export.h>

namespace brillo {

// Provides a simple Fake to use for testing modules that use CrosConfig.
// It allows configuration responses to be set up in the test for use in the
// module.
class BRILLO_EXPORT FakeCrosConfig : public CrosConfigInterface {
 public:
  FakeCrosConfig();
  ~FakeCrosConfig() override;

  // Sets up string to return for a future call to GetString().
  // @path: Path to property (e.g. "/")
  // @prop: Name of property to set up
  // @val: The string to return when this property is looked up.
  void SetString(const std::string& path,
                 const std::string& prop,
                 const std::string& val);

  // CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out) override;

  // Sets up an entry in the target directory map. This will be used in future
  // calls to GetAbsPath(). The Fake does not read the schema file, relying
  // instead on its caller to provide test data. This provides more flexibility
  // for callers to implement tests as they wish.
  // @prop: Name of property to set up
  // @dirname: Directory name to use for that property
  void SetTargetDir(const std::string& prop, const std::string& dirname);

  // CrosConfigInterface:
  bool GetAbsPath(const std::string& path,
                  const std::string& prop,
                  std::string* val_out) override;

 private:
  using PathProp = std::pair<std::string, std::string>;

  // This stores the string values, keyed by a pair consisting of the path
  // and the property.
  std::map<PathProp, std::string> values_;
  std::map<std::string, std::string> target_dirs_;

  DISALLOW_COPY_AND_ASSIGN(FakeCrosConfig);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_FAKE_CROS_CONFIG_H_
