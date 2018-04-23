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

  // CrosConfigImpl:
  bool SelectConfigByIdentityX86(
      const CrosConfigIdentityX86& identity) override;
  bool ReadConfigFile(const base::FilePath& filepath) override;
  bool GetString(const std::string& path,
                 const std::string& prop,
                 std::string* val_out,
                 std::vector<std::string>* log_msgs_out) override;

 private:
  std::unique_ptr<const base::Value> json_config_;
  // Owned by json_config_
  const base::DictionaryValue* model_dict_;  // Model root

  DISALLOW_COPY_AND_ASSIGN(CrosConfigJson);
};

}  // namespace brillo

#endif  // CHROMEOS_CONFIG_LIBCROS_CONFIG_CROS_CONFIG_JSON_H_
