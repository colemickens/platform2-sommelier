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

bool CrosConfig::GetString(const std::string& path, const std::string& prop,
                           std::string* val) {
  if (!InitCheck()) {
    return false;
  }

  if (model_offset_ < 0) {
    LOG(ERROR) << "Please specify the model to access.";
    return false;
  }

  if (path.size() == 0) {
    LOG(ERROR) << "Path must be specified";
    return false;
  }

  if (path.substr(0, 1) != "/") {
    LOG(ERROR) << "Path must start with / specifying the root node";
    return false;
  }

  if (root_dict_ != nullptr) {
    const base::ListValue* models_list = nullptr;
    if (root_dict_->GetList("models", &models_list)) {
      size_t num_models = models_list->GetSize();
      for (size_t i = 0; i < num_models; ++i) {
        const base::DictionaryValue* model_dict = nullptr;
        if (models_list->GetDictionary(i, &model_dict)) {
          std::string model_name;
          if (model_dict->GetString("name", &model_name)) {
            if (model_name == model_) {
              bool valid_path = true;
              const base::DictionaryValue* attr_dict = model_dict;

              std::vector<std::string> path_tokens = base::SplitString(
                  path.substr(1),
                  "/",
                  base::TRIM_WHITESPACE,
                  base::SPLIT_WANT_ALL);
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
            }
          }
        }
      }
    }
  }

  return false;
}

bool CrosConfig::GetAbsPath(const std::string& path, const std::string& prop,
                            std::string* val_out) {
  std::string val;
  if (!GetString(path, prop, &val)) {
    return false;
  }

  // TODO(sjg@chromium.org): Look up the absolute path.
  return false;
}

bool CrosConfig::InitCommon(const base::FilePath& config_filepath,
                            const std::string& name, int sku_id,
                            const std::string& whitelabel_name) {
  std::string config_json_data;
  if (!base::ReadFileToString(config_filepath, &config_json_data)) {
    LOG(ERROR) << "Could not read file " << config_filepath.MaybeAsASCII();
    return false;
  }
  std::string error_msg;
  json_config_ =
      base::JSONReader::ReadAndReturnError(config_json_data,
                                            base::JSON_PARSE_RFC,
                                            nullptr /* error_code_out */,
                                            &error_msg,
                                            nullptr /* error_line_out */,
                                            nullptr /* error_column_out */);
  if (!json_config_) {
    LOG(ERROR) << "Fail to parse config.json: " << error_msg;
    return false;
  }
  if (!json_config_->GetAsDictionary(&root_dict_)) {
    LOG(ERROR) << "Fail to parse root dictionary from "
                << config_filepath.MaybeAsASCII();
    return false;
  }
  if (!SelectModelConfigByIDs(name, sku_id, whitelabel_name)) {
    LOG(ERROR) << "Cannot find SKU for name " << name << " SKU ID " << sku_id;
    return false;
  }
  inited_ = true;

  return true;
}

}  // namespace brillo
