// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Libary to provide access to the Chrome OS master configuration in YAML / JSON
// format

#include "chromeos-config/libcros_config/cros_config_yaml.h"

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

namespace {
const char kConfigJsonPath[] = "/usr/share/chromeos-config/config.json";
}  // namespace

namespace brillo {

CrosConfigYaml::CrosConfigYaml() {}

CrosConfigYaml::~CrosConfigYaml() {}

bool CrosConfigYaml::InitModel() {
  return InitForConfig(base::FilePath(kConfigJsonPath));
}

std::string CrosConfigYaml::GetFullPath(ConfigNode node) {
  // TODO(sjg@chromium.org): Figure out how to get the path to a node
  return "TODO";
}

ConfigNode CrosConfigYaml::GetPathNode(ConfigNode base_node,
                                       const std::string& path) {
  const base::DictionaryValue* attr_dict = base_node.GetDict();

  std::vector<std::string> path_tokens = base::SplitString(
      path.substr(1), "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  bool valid_path = true;
  for (std::string& path : path_tokens) {
    valid_path = attr_dict->GetDictionary(path, &attr_dict);
    if (!valid_path) {
      break;
    }
  }

  if (!valid_path) {
    return ConfigNode();
  }
  return ConfigNode(attr_dict);
}

bool CrosConfigYaml::LookupPhandle(ConfigNode node,
                                   const std::string& prop_name,
                                   ConfigNode* node_out) {
  const base::DictionaryValue* target = nullptr;
  if (!node.GetDict()->GetDictionary(prop_name, &target)) {
    return false;
  }
  *node_out = ConfigNode(target);
  return true;
}

bool CrosConfigYaml::SelectModelConfigByIDs(
    const std::string& find_name,
    int find_sku_id,
    const std::string& find_whitelabel_name) {
  const base::DictionaryValue* root_dict = nullptr;
  if (json_config_->GetAsDictionary(&root_dict)) {
    const base::DictionaryValue* chromeos = nullptr;
    if (root_dict->GetDictionary("chromeos", &chromeos)) {
      const base::ListValue* models_list = nullptr;
      if (chromeos->GetList("models", &models_list)) {
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
              bool customization_id_match = true;
              std::string current_customization_id;
              if (identity_dict->GetString("customization-id",
                                           &current_customization_id) &&
                  !current_customization_id.empty()) {
                customization_id_match =
                    current_customization_id == find_whitelabel_name;
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
  }
  if (!model_dict_) {
    CROS_CONFIG_LOG(ERROR) << "Failed to find config for name: " << find_name
                           << " sku_id: " << find_sku_id
                           << " customization_id: " << find_whitelabel_name;
    return false;
  } else if (!model_dict_->GetString("name", &model_name_)) {
    CROS_CONFIG_LOG(ERROR) << "Mode does not have a name";
    return false;
  }
  model_node_ = ConfigNode(model_dict_);
  std::string wallpaper;
  bool ok = model_dict_->GetString("wallpaper", &wallpaper);
  CROS_CONFIG_LOG(INFO) << "wallpaper" << ok << wallpaper;
  CROS_CONFIG_LOG(INFO) << "init dict" << model_node_.GetDict();
  return true;
}

int CrosConfigYaml::GetProp(const ConfigNode& node,
                            std::string name,
                            std::string* value_out) {
  CROS_CONFIG_LOG(INFO) << "lookup dict" << model_node_.GetDict();
  if (node.GetDict()->GetString(name, value_out)) {
    CROS_CONFIG_LOG(INFO) << "got value for '" << name
                          << "' :'" << *value_out << "'";
    return value_out->size();
  }
  return -1;
}

bool CrosConfigYaml::ReadConfigFile(const base::FilePath& filepath) {
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

  // TODO(sjg): These should not be needed once we adjust the yaml to pull in
  // references with <<< or similar.
  phandle_props_.push_back("arc-properties-type");
  phandle_props_.push_back("audio-type");
  phandle_props_.push_back("bcs-type");
  phandle_props_.push_back("power-type");
  phandle_props_.push_back("shares");
  phandle_props_.push_back("single-sku");
  phandle_props_.push_back("touch-type");
  phandle_props_.push_back("whitelabel");

  return true;
}

}  // namespace brillo
