// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"
#include "chromeos-config/libcros_config/cros_config_impl.h"
#include "chromeos-config/libcros_config/identity.h"

// TODO(sjg@chromium.org): See if this can be accepted upstream.
extern "C" {
#include <libfdt.h>
};
#include <stdlib.h>

#include <iostream>
#include <sstream>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace {
const char kSmbiosTablePath[] = "/run/cros_config/SMBIOS";
const char kCustomizationId[] = "/sys/firmware/vpd/ro/customization_id";
}  // namespace

namespace brillo {

ConfigNode::ConfigNode() {
  valid_ = false;
}

ConfigNode::ConfigNode(int offset) {
  valid_ = true;
  node_offset_ = offset;
}

ConfigNode::ConfigNode(const base::DictionaryValue* dict) {
  valid_ = true;
  dict_ = dict;
}

bool ConfigNode::IsValid() const {
  return valid_;
}

int ConfigNode::GetOffset() const {
  if (!valid_) {
    return -1;
  }
  return node_offset_;
}

const base::DictionaryValue* ConfigNode::GetDict() const {
  if (!valid_) {
    return 0;
  }
  return dict_;
}

bool ConfigNode::operator==(const ConfigNode& other) const {
  if (valid_ != other.valid_) {
    return false;
  }
  if (node_offset_ != other.node_offset_) {
    return false;
  }
  return true;
}

bool CrosConfigInterface::IsLoggingEnabled() {
  static const char* logging_var = getenv("CROS_CONFIG_DEBUG");
  static bool enabled = logging_var && *logging_var;
  return enabled;
}

CrosConfigImpl::CrosConfigImpl() {}

CrosConfigImpl::~CrosConfigImpl() {}

bool CrosConfigImpl::InitForConfig(const base::FilePath& filepath) {
  base::FilePath smbios_file(kSmbiosTablePath);
  base::FilePath vpd_file(kCustomizationId);
  return InitCommon(filepath, smbios_file, vpd_file);
}

bool CrosConfigImpl::InitCheck() const {
  if (!inited_) {
    CROS_CONFIG_LOG(ERROR)
        << "Init*() must be called before accessing configuration";
    return false;
  }
  return true;
}

bool CrosConfigImpl::GetString(ConfigNode base_node,
                               const std::string& path,
                               const std::string& prop,
                               std::string* val_out,
                               std::vector<std::string>* log_msgs_out) {
  ConfigNode subnode = GetPathNode(base_node, path);
  ConfigNode wl_subnode;
  if (whitelabel_node_.IsValid()) {
    wl_subnode = GetPathNode(whitelabel_node_, path);
    if (!subnode.IsValid() && wl_subnode.IsValid()) {
      CROS_CONFIG_LOG(INFO)
          << "The path " << GetFullPath(base_node) << path
          << " does not exist. Falling back to whitelabel path";
      subnode = wl_subnode;
    }
  }
  if (!subnode.IsValid()) {
    log_msgs_out->push_back("The path " + GetFullPath(base_node) + path +
                            " does not exist.");
    return false;
  }

  std::string value;
  int len = GetProp(subnode, prop, &value);
  if (len < 0 && wl_subnode.IsValid()) {
    len = GetProp(wl_subnode, prop, &value);
    CROS_CONFIG_LOG(INFO) << "The property " << prop
                          << " does not exist. Falling back to "
                          << "whitelabel property";
  }
  if (len < 0) {
    ConfigNode target_node;
    for (int i = 0; i < phandle_props_.size(); i++) {
      LookupPhandle(subnode, phandle_props_[i], &target_node);
      if (target_node.IsValid()) {
        len = GetProp(target_node, prop, &value);
        if (len < 0) {
          CROS_CONFIG_LOG(INFO)
              << "Followed " << phandle_props_[i] << " phandle";
          break;
        }
      }
    }
  }

  if (len < 0) {
    log_msgs_out->push_back("Cannot get path " + path + " property " + prop +
                            ": " + "full path " + GetFullPath(subnode) + ": " +
                            fdt_strerror(len));
    return false;
  }

  // We must have a normally terminated string. This guards against a string
  // list being used, or perhaps a property that does not contain a valid
  // string at all.
  if (!len || value.size() != len) {
    log_msgs_out->push_back("String at path " + path + " property " + prop +
                            " is invalid");
    return false;
  }

  val_out->assign(value);

  return true;
}

bool CrosConfigImpl::GetString(const std::string& path,
                               const std::string& prop,
                               std::string* val_out,
                               std::vector<std::string>* log_msgs_out) {
  if (!InitCheck()) {
    return false;
  }

  if (!model_node_.IsValid()) {
    log_msgs_out->push_back("Please specify the model to access.");
    return false;
  }

  if (path.size() == 0) {
    log_msgs_out->push_back("Path must be specified");
    return false;
  }

  if (path.substr(0, 1) != "/") {
    log_msgs_out->push_back("Path must start with / specifying the root node");
    return false;
  }

  if (whitelabel_tag_node_.IsValid()) {
    if (path == "/" &&
        GetString(whitelabel_tag_node_, "/", prop, val_out, log_msgs_out)) {
      return true;
    }
    // TODO(sjg@chromium.org): We are considering moving the key-id to the root
    // of the model schema. If we do, we can drop this special case.
    if (path == "/firmware" && prop == "key-id" &&
        GetString(whitelabel_tag_node_, "/", prop, val_out, log_msgs_out)) {
      return true;
    }
  }
  if (!GetString(model_node_, path, prop, val_out, log_msgs_out)) {
    if (submodel_node_.IsValid() &&
        GetString(submodel_node_, path, prop, val_out, log_msgs_out)) {
      return true;
    }
    for (ConfigNode node : default_nodes_) {
      if (GetString(node, path, prop, val_out, log_msgs_out))
        return true;
    }
    return false;
  }
  return true;
}

bool CrosConfigImpl::GetString(const std::string& path,
                               const std::string& prop,
                               std::string* val_out) {
  std::vector<std::string> log_msgs;
  if (!GetString(path, prop, val_out, &log_msgs)) {
    for (std::string msg : log_msgs) {
      CROS_CONFIG_LOG(ERROR) << msg;
    }
    return false;
  }
  return true;
}

bool CrosConfigImpl::GetAbsPath(const std::string& path,
                                const std::string& prop,
                                std::string* val_out) {
  std::string val;
  if (!GetString(path, prop, &val)) {
    return false;
  }

  if (target_dirs_.find(prop) == target_dirs_.end()) {
    CROS_CONFIG_LOG(ERROR) << "Absolute path requested at path " << path
                           << " property " << prop << ": not found";
    return false;
  }
  *val_out = target_dirs_[prop] + "/" + val;

  return true;
}

bool CrosConfigImpl::InitCommon(const base::FilePath& filepath,
                                const base::FilePath& mem_file,
                                const base::FilePath& vpd_file) {
  // Many systems will not have a config database (yet), so just skip all the
  // setup without any errors if the config file doesn't exist.
  if (!base::PathExists(filepath)) {
    return false;
  }

  ReadConfigFile(filepath);

  CrosConfigIdentity identity;
  std::string name, customization_id;
  int sku_id;
  if (!identity.ReadIdentity(mem_file, vpd_file, &name, &sku_id,
                             &customization_id)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot read identity";
    return false;
  }
  if (!SelectModelConfigByIDs(name, sku_id, customization_id)) {
    CROS_CONFIG_LOG(ERROR) << "Cannot find SKU for name " << name << " SKU ID "
                           << sku_id;
    return false;
  }

  // See if there is a whitelabel config for this model.
  if (!whitelabel_node_.IsValid()) {
    LookupPhandle(model_node_, "whitelabel", &whitelabel_node_);
  }
  ConfigNode next_node;
  default_nodes_.clear();
  for (ConfigNode node = model_node_;
       LookupPhandle(node, "default", &next_node); node = next_node) {
    if (std::find(default_nodes_.begin(), default_nodes_.end(), next_node) !=
        default_nodes_.end()) {
      CROS_CONFIG_LOG(ERROR) << "Circular default at " << GetFullPath(node);
      return false;
    }
    default_nodes_.push_back(next_node);
  }

  CROS_CONFIG_LOG(INFO) << "Using master configuration for model "
                        << model_name_ << ", submodel "
                        << (submodel_name_.empty() ? "(none)" : submodel_name_);
  if (whitelabel_node_.IsValid()) {
    CROS_CONFIG_LOG(INFO) << "Whitelabel of  " << GetFullPath(whitelabel_node_);
  } else if (whitelabel_tag_node_.IsValid()) {
    CROS_CONFIG_LOG(INFO) << "Whitelabel tag "
                          << GetFullPath(whitelabel_tag_node_);
  }
  inited_ = true;

  return true;
}

}  // namespace brillo
