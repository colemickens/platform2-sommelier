// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Libary to provide access to the Chrome OS master configuration in YAML / JSON
// format

#include "chromeos-config/libcros_config/cros_config_json.h"

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/values.h>
#include "chromeos-config/libcros_config/identity_arm.h"
#include "chromeos-config/libcros_config/identity_x86.h"

namespace brillo {

CrosConfigJson::CrosConfigJson() : config_dict_(nullptr) {}

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
      const base::ListValue* configs_list = nullptr;
      if (chromeos->GetList("configs", &configs_list)) {
        size_t num_configs = configs_list->GetSize();
        for (size_t i = 0; i < num_configs; ++i) {
          const base::DictionaryValue* config_dict = nullptr;
          if (configs_list->GetDictionary(i, &config_dict)) {
            const base::DictionaryValue* identity_dict = nullptr;
            if (config_dict->GetDictionary("identity", &identity_dict)) {
              int find_sku_id = -1;
              bool platform_specific_match = false;
              if (identity_x86) {
                find_sku_id = identity_x86->GetSkuId();

                const std::string& find_name = identity_x86->GetName();
                std::string current_name;
                if (identity_dict->GetString("smbios-name-match",
                                             &current_name)) {
                  platform_specific_match = current_name == find_name;
                }
              } else if (identity_arm) {
                find_sku_id = identity_arm->GetSkuId();

                std::string current_dt_compatible_match;
                if (identity_dict->GetString("device-tree-compatible-match",
                                             &current_dt_compatible_match)) {
                  platform_specific_match =
                      identity_arm->IsCompatible(current_dt_compatible_match);
                }
              }
              bool sku_match = true;
              int current_sku_id;
              if (find_sku_id > -1 &&
                  identity_dict->GetInteger("sku-id", &current_sku_id)) {
                sku_match = current_sku_id == find_sku_id;
              }

              std::string current_vpd_tag = "";
              identity_dict->GetString("whitelabel-tag", &current_vpd_tag);
              if (current_vpd_tag.empty()) {
                identity_dict->GetString("customization-id", &current_vpd_tag);
              }
              // Currently, the find_whitelabel_name can be either the
              // whitelabel-tag or the customization-id.
              bool vpd_tag_match = current_vpd_tag == find_whitelabel_name;

              if (platform_specific_match && sku_match && vpd_tag_match) {
                config_dict_ = config_dict;
                break;
              }
            }
          }
        }
      }
    }
  }
  if (!config_dict_) {
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
                               std::string* val_out) {
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
  const base::DictionaryValue* attr_dict = config_dict_;

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

  if (valid_path) {
    std::string value;
    if (attr_dict->GetString(prop, &value)) {
      val_out->assign(value);
      return true;
    }

    int int_value;
    if (attr_dict->GetInteger(prop, &int_value)) {
      val_out->assign(std::to_string(int_value));
      return true;
    }

    bool bool_value;
    if (attr_dict->GetBoolean(prop, &bool_value)) {
      if (bool_value) {
        val_out->assign("true");
      } else {
        val_out->assign("false");
      }
      return true;
    }
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

  return true;
}

}  // namespace brillo
