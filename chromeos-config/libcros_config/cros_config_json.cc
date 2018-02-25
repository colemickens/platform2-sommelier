// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Libary to provide access to the Chrome OS master configuration in JSON format

#include "chromeos-config/libcros_config/cros_config.h"

#include <iostream>
#include <sstream>
#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/process/launch.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_util.h>
#include <base/strings/string_split.h>
#include <base/values.h>

namespace {
const char kConfigJsonPath[] = "/usr/share/chromeos-config/config.json";
}  // namespace

namespace brillo {

bool CrosConfig::InitModel() {
  return InitForConfig(base::FilePath(kConfigJsonPath));
}

bool CrosConfig::GetString(const std::string& path,
                           const std::string& prop,
                           std::string* val) {
  if (!InitCheck()) {
    return false;
  }

  if (path.size() == 0) {
    CROS_CONFIG_LOG(ERROR) << "Path must be specified";
    return false;
  }

  if (path.substr(0, 1) != "/") {
    CROS_CONFIG_LOG(ERROR) << "Path must start with / specifying the root node";
    return false;
  }

  bool valid_path = true;
  const base::DictionaryValue* attr_dict = model_dict_;

  std::vector<std::string> path_tokens = base::SplitString(
      path.substr(1), "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (std::string& path : path_tokens) {
    valid_path = attr_dict->GetDictionary(path, &attr_dict);
    if (!valid_path) {
      break;
    }
  }

  std::string value;
  if (valid_path && attr_dict->GetString(prop, &value)) {
    val->assign(value);
    return true;
  }

  return false;
}

bool CrosConfig::GetAbsPath(const std::string& path,
                            const std::string& prop,
                            std::string* val_out) {
  std::string val;
  if (!GetString(path, prop, &val)) {
    return false;
  }

  // TODO(sjg@chromium.org): Look up the absolute path.
  return false;
}

bool CrosConfig::InitCommon(const base::FilePath& config_filepath,
                            const std::string& name,
                            int sku_id,
                            const std::string& customization_id) {
  std::string config_json_data;
  if (!base::ReadFileToString(config_filepath, &config_json_data)) {
    CROS_CONFIG_LOG(ERROR) << "Could not read file "
                           << config_filepath.MaybeAsASCII();
    return false;
  }
  std::string error_msg;
  json_config_ = base::JSONReader::ReadAndReturnError(
      config_json_data, base::JSON_PARSE_RFC, nullptr /* error_code_out */,
      &error_msg, nullptr /* error_line_out */, nullptr /* error_column_out */);
  if (!json_config_) {
    CROS_CONFIG_LOG(ERROR) << "Fail to parse config.json: " << error_msg;
    return false;
  }

  const base::DictionaryValue* root_dict = nullptr;
  if (json_config_->GetAsDictionary(&root_dict)) {
    const base::ListValue* models_list = nullptr;
    if (root_dict->GetList("models", &models_list)) {
      size_t num_models = models_list->GetSize();
      for (size_t i = 0; i < num_models; ++i) {
        const base::DictionaryValue* model_dict = nullptr;
        if (models_list->GetDictionary(i, &model_dict)) {
          const base::DictionaryValue* identity_dict = nullptr;
          if (model_dict->GetDictionary("identity", &identity_dict)) {
            bool require_sku_match = sku_id > -1;
            int current_sku_id;
            bool sku_match =
                (!require_sku_match ||
                 (identity_dict->GetInteger("sku-id", &current_sku_id) &&
                  current_sku_id == sku_id));

            bool name_match = true;
            std::string current_name;
            if (identity_dict->GetString("smbios-name-match", &current_name) &&
                !name.empty()) {
              name_match = current_name == name;
            }
            bool customization_id_match = true;
            std::string current_customization_id;
            if (identity_dict->GetString("customization-id",
                                         &current_customization_id) &&
                !current_customization_id.empty()) {
              customization_id_match =
                  current_customization_id == customization_id;
            }

            if (sku_match && name_match && customization_id_match) {
              model_dict_ = model_dict;
              break;
            }
          }
        }
      }
    }
  }
  if (!model_dict_) {
    CROS_CONFIG_LOG(ERROR) << "Failed to find config for name: " << name
                           << " sku_id: " << sku_id
                           << " customization_id: " << customization_id;
    return false;
  }
  inited_ = true;

  return true;
}

}  // namespace brillo
