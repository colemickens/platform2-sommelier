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

bool CrosConfigJson::SelectConfigByIdentity(
    const CrosConfigIdentityArm* identity_arm,
    const CrosConfigIdentityX86* identity_x86) {
  const CrosConfigIdentity* identity;
  if (identity_arm) {
    identity = identity_arm;
  } else {
    identity = identity_x86;
  }
  const std::string& find_whitelabel_name = identity->GetVpdId();

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
              bool platform_specific_match = false;
              if (identity_x86) {
                const std::string& find_name = identity_x86->GetName();
                int find_sku_id = identity_x86->GetSkuId();
                int current_sku_id;
                bool sku_match = true;
                if (find_sku_id > -1 &&
                    identity_dict->GetInteger("sku-id", &current_sku_id)) {
                  sku_match = current_sku_id == find_sku_id;
                }

                bool name_match = true;
                std::string current_name;
                if (identity_dict->GetString("smbios-name-match",
                                             &current_name) &&
                    !find_name.empty()) {
                  name_match = current_name == find_name;
                }
                platform_specific_match = sku_match && name_match;
              } else if (identity_arm) {
                bool dt_compatible_match = false;
                std::string current_dt_compatible_match;
                if (identity_dict->GetString("device-tree-compatible-match",
                                             &current_dt_compatible_match)) {
                  dt_compatible_match =
                      identity_arm->IsCompatible(current_dt_compatible_match);
                }
                platform_specific_match = dt_compatible_match;
              }

              bool vpd_tag_match = true;
              std::string current_vpd_tag;
              if ((identity_dict->GetString("whitelabel-tag",
                                           &current_vpd_tag) ||
                  identity_dict->GetString("customization-id",
                                           &current_vpd_tag)) &&
                  !current_vpd_tag.empty()) {
                // Currently, the find_whitelabel_name can be either the
                // whitelabel-tag or the customization-id.
                vpd_tag_match = current_vpd_tag == find_whitelabel_name;
              }

              if (platform_specific_match && vpd_tag_match) {
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
    if (identity_arm) {
      CROS_CONFIG_LOG(ERROR)
          << "Failed to find config for device-tree compatible string: "
          << identity_arm->GetCompatibleDeviceString();
    } else {
      CROS_CONFIG_LOG(ERROR)
          << "Failed to find config for name: " << identity_x86->GetName()
          << " sku_id: " << identity_x86->GetSkuId()
          << " customization_id: " << find_whitelabel_name;
    }
    return false;
  }
  inited_ = true;
  return true;
}

bool CrosConfigJson::SelectConfigByIdentityArm(
    const CrosConfigIdentityArm& identity) {
  return SelectConfigByIdentity(&identity, NULL);
}
bool CrosConfigJson::SelectConfigByIdentityX86(
    const CrosConfigIdentityX86& identity) {
  return SelectConfigByIdentity(NULL, &identity);
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
