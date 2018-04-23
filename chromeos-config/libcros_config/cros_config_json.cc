// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Libary to provide access to the Chrome OS master configuration in YAML / JSON
// format

#include "chromeos-config/libcros_config/cros_config_json.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

namespace brillo {

CrosConfigJson::CrosConfigJson() : model_dict_(nullptr) {}

CrosConfigJson::~CrosConfigJson() {}

bool CrosConfigJson::SelectConfigByIdentityX86(
    const CrosConfigIdentityX86& identity) {
  const std::string& find_name = identity.GetName();
  int find_sku_id = identity.GetSkuId();
  const std::string& find_whitelabel_name = identity.GetCustomizationId();
  const base::DictionaryValue* root_dict = nullptr;
  if (json_config_->GetAsDictionary(&root_dict)) {
    const base::DictionaryValue* chromeos = nullptr;
    if (root_dict->GetDictionary("chromeos", &chromeos)) {
      const base::ListValue* models_list = nullptr;
      if (chromeos->GetList("configs", &models_list) ||
          chromeos->GetList("models", &models_list)) {
        size_t num_models = models_list->GetSize();
        for (size_t i = 0; i < num_models; ++i) {
          const base::DictionaryValue* model_dict = nullptr;
          if (models_list->GetDictionary(i, &model_dict)) {
            const base::DictionaryValue* identity_dict = nullptr;
            if (model_dict->GetDictionary("identity", &identity_dict)) {
              bool require_sku_match = find_sku_id > -1;
              int current_sku_id;
              bool sku_match =
                  (!require_sku_match ||
                   (identity_dict->GetInteger("sku-id", &current_sku_id) &&
                    current_sku_id == find_sku_id));

              bool name_match = true;
              std::string current_name;
              if (identity_dict->GetString("smbios-name-match",
                                           &current_name) &&
                  !find_name.empty()) {
                name_match = current_name == find_name;
              }
              bool whitelabel_tag_match = true;
              std::string current_whitelabel_tag;
              if (identity_dict->GetString("whitelabel-tag",
                                           &current_whitelabel_tag) &&
                  !current_whitelabel_tag.empty()) {
                whitelabel_tag_match =
                    current_whitelabel_tag == find_whitelabel_name;
              }

              bool customization_id_match = true;
              std::string current_customization_id;
              if (identity_dict->GetString("customization-id",
                                           &current_customization_id) &&
                  !current_customization_id.empty()) {
                // Currently, the find_whitelabel_name can be either the
                // whitelabel-tag or the customization-id.
                customization_id_match =
                    current_customization_id == find_whitelabel_name;
              }

              if (sku_match && name_match && whitelabel_tag_match &&
                  customization_id_match) {
                model_dict_ = model_dict;
                break;
              }
            }
          }
        }
      }
    }
  }
  if (!model_dict_) {
    CROS_CONFIG_LOG(ERROR) << "Failed to find config for name: " << find_name
                           << " sku_id: " << find_sku_id
                           << " customization_id: " << find_whitelabel_name;
    return false;
  }
  inited_ = true;
  return true;
}

bool CrosConfigJson::GetString(const std::string& path,
                               const std::string& prop,
                               std::string* val_out,
                               std::vector<std::string>* log_msgs_out) {
  if (!InitCheck()) {
    return false;
  }

  if (path.empty()) {
    LOG(ERROR) << "Path must be specified";
    return false;
  }

  if (path[0] != '/') {
    LOG(ERROR) << "Path must start with / specifying the root node";
    return false;
  }

  bool valid_path = true;
  const base::DictionaryValue* attr_dict = model_dict_;

  if (path.length() > 1) {
    std::vector<std::string> path_tokens = base::SplitString(
        path.substr(1), "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const std::string& path : path_tokens) {
      valid_path = attr_dict->GetDictionary(path, &attr_dict);
      if (!valid_path) {
        CROS_CONFIG_LOG(ERROR) << "Failed to find path: " << path;
        break;
      }
    }
  }

  std::string value;
  if (valid_path && attr_dict->GetString(prop, &value)) {
    val_out->assign(value);
    return true;
  }
  return false;
}

bool CrosConfigJson::ReadConfigFile(const base::FilePath& filepath) {
  std::string json_data;
  if (!base::ReadFileToString(filepath, &json_data)) {
    CROS_CONFIG_LOG(ERROR) << "Could not read file " << filepath.MaybeAsASCII();
    return false;
  }
  std::string error_msg;
  json_config_ = base::JSONReader::ReadAndReturnError(
      json_data, base::JSON_PARSE_RFC, nullptr /* error_code_out */, &error_msg,
      nullptr /* error_line_out */, nullptr /* error_column_out */);
  if (!json_config_) {
    CROS_CONFIG_LOG(ERROR) << "Fail to parse config.json: " << error_msg;
    return false;
  }

  // TODO(crbug.com/xxx): Figure out a way to represent this. For now it is
  // hard-coded
  target_dirs_["alsa-conf"] = "/usr/share/alsa/ucm";
  target_dirs_["cras-config-dir"] = "/etc/cras";
  target_dirs_["dptf-dv"] = "/etc/dptf";
  target_dirs_["dsp-ini"] = "/etc/cras";
  target_dirs_["hifi-conf"] = "/usr/share/alsa/ucm";
  target_dirs_["topology-bin"] = "/lib/firmware";
  target_dirs_["volume"] = "/etc/cras";

  return true;
}

}  // namespace brillo
