// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_JSON_H_
#define CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_JSON_H_

#include "chromeos-config/libcros_config/cros_config_impl.h"

#include <memory>
#include <string>
#include <vector>

namespace brillo {

// JSON implementation of master configuration
class CrosConfigJson : public CrosConfigImpl {
 public:
  CrosConfigJson();
  ~CrosConfigJson() override;

  // CrosConfigInterface:
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out) override;

  // CrosConfigImpl:
  bool SelectConfigByIdentityArm(
      const CrosConfigIdentityArm& identity) override;
  bool SelectConfigByIdentityX86(
      const CrosConfigIdentityX86& identity) override;
  bool ReadConfigFile(const base::FilePath& filepath) override;

 private:
  // Common impl for both the X86 and ARM based identity schemes.
  // Shares all of the basic logic for iterating through configs;
  // however, performs slight variations on identity matching based
  // on the X86 versus ARM identity attributes.
  bool SelectConfigByIdentity(const CrosConfigIdentityArm* identity_arm,
                              const CrosConfigIdentityX86* identity_x86);
  std::unique_ptr<const base::Value> json_config_;
  // Owned by json_config_
  const base::DictionaryValue* config_dict_;  // Root of configs

  DISALLOW_COPY_AND_ASSIGN(CrosConfigJson);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_JSON_H_
