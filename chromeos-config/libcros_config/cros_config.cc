// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"
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
const char kConfigDtbPath[] = "/usr/share/chromeos-config/config.dtb";
const char kTargetDirsPath[] = "/chromeos/schema/target-dirs";
const char kSchemaPath[] = "/chromeos/schema";
const char kPhandleProperties[] = "phandle-properties";
}  // namespace

namespace brillo {

bool CrosConfigInterface::IsLoggingEnabled() {
  static const char* logging_var = getenv("CROS_CONFIG_DEBUG");
  static bool enabled = logging_var && *logging_var;
  return enabled;
}

bool CrosConfig::InitModel() {
  return InitForConfig(base::FilePath(kConfigDtbPath));
}

std::string CrosConfig::GetFullPath(int offset) {
  const void* blob = blob_.c_str();
  char buf[256];
  int err;

  err = fdt_get_path(blob, offset, buf, sizeof(buf));
  if (err) {
    CROS_CONFIG_LOG(WARNING) << "Cannot get full path: " << fdt_strerror(err);
    return "unknown";
  }

  return std::string(buf);
}

int CrosConfig::GetPathOffset(int base_offset, const std::string& path) {
  const void* blob = blob_.c_str();
  auto parts = base::SplitString(path.substr(1), "/", base::KEEP_WHITESPACE,
                                 base::SPLIT_WANT_ALL);
  int offset = base_offset;
  for (auto part : parts) {
    offset = fdt_subnode_offset(blob, offset, part.c_str());
    if (offset < 0) {
      break;
    }
  }
  return offset;
}

bool CrosConfig::GetString(int base_offset, const std::string& path,
                           const std::string& prop, std::string* val_out,
                           std::vector<std::string>* log_msgs_out) {
  const void* blob = blob_.c_str();

  int subnode = GetPathOffset(base_offset, path);
  int wl_subnode = -1;
  if (whitelabel_offset_ != -1) {
    wl_subnode = GetPathOffset(whitelabel_offset_, path);
    if (subnode < 0 && wl_subnode >= 0) {
      CROS_CONFIG_LOG(INFO)
          << "The path " << GetFullPath(base_offset) << path
          << " does not exist. Falling back to whitelabel path";
      subnode = wl_subnode;
    }
  }
  if (subnode < 0) {
    log_msgs_out->push_back("The path " + GetFullPath(base_offset) + path
                           + " does not exist.");
    return false;
  }

  int len = 0;
  const char* ptr = static_cast<const char*>(
      fdt_getprop(blob, subnode, prop.c_str(), &len));
  if (!ptr && wl_subnode >= 0) {
    ptr = static_cast<const char*>(fdt_getprop(blob, wl_subnode, prop.c_str(),
                                               &len));
    CROS_CONFIG_LOG(INFO) << "The property " << prop
                          << " does not exist. Falling back to "
                          << "whitelabel property";
  }
  if (!ptr) {
    int target_node;
    for (int i = 0; i < phandle_props_.size(); i++) {
      LookupPhandle(subnode, phandle_props_[i], &target_node);
      if (target_node >= 0) {
        ptr = static_cast<const char*>(
            fdt_getprop(blob, target_node, prop.c_str(), &len));
        if (ptr) {
          CROS_CONFIG_LOG(INFO)
              << "Followed " << phandle_props_[i] << " phandle";
          break;
        }
      }
    }
  }

  if (!ptr || len < 0) {
    log_msgs_out->push_back("Cannot get path " + path + " property " + prop
        + ": " + "full path " + GetFullPath(subnode) + ": "
        + fdt_strerror(len));
    return false;
  }

  // We must have a normally terminated string. This guards against a string
  // list being used, or perhaps a property that does not contain a valid
  // string at all.
  if (!len || strnlen(ptr, len) != static_cast<size_t>(len - 1)) {
    log_msgs_out->push_back("String at path " + path + " property " + prop
                           + " is invalid");
    return false;
  }

  val_out->assign(ptr);

  return true;
}

bool CrosConfig::GetString(const std::string& path, const std::string& prop,
                           std::string* val_out,
                           std::vector<std::string>* log_msgs_out) {
  if (!InitCheck()) {
    return false;
  }

  if (model_offset_ < 0) {
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

  if (whitelabel_tag_offset_ != -1) {
    if (path == "/" &&
        GetString(whitelabel_tag_offset_, "/", prop, val_out, log_msgs_out)) {
      return true;
    }
    // TODO(sjg@chromium.org): We are considering moving the key-id to the root
    // of the model schema. If we do, we can drop this special case.
    if (path == "/firmware" && prop == "key-id" &&
        GetString(whitelabel_tag_offset_, "/", prop, val_out, log_msgs_out)) {
      return true;
    }
  }
  if (!GetString(model_offset_, path, prop, val_out, log_msgs_out)) {
    if (submodel_offset_ != -1 &&
        GetString(submodel_offset_, path, prop, val_out, log_msgs_out)) {
      return true;
    }
    for (int offset : default_offsets_) {
      if (GetString(offset, path, prop, val_out, log_msgs_out))
        return true;
    }
    return false;
  }
  return true;
}

bool CrosConfig::GetString(const std::string& path, const std::string& prop,
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

bool CrosConfig::GetAbsPath(const std::string& path, const std::string& prop,
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

bool CrosConfig::LookupPhandle(int node_offset, const std::string& prop_name,
                               int* offset_out) {
  const void* blob = blob_.c_str();
  int len;
  const fdt32_t* ptr = static_cast<const fdt32_t*>(
      fdt_getprop(blob, node_offset, prop_name.c_str(), &len));

  // We probably don't need all these checks since validation will ensure that
  // the config is correct. But this is a critical tool and we want to avoid
  // crashes in any situation.
  *offset_out = -1;
  if (!ptr) {
    return false;
  }
  if (len != sizeof(fdt32_t)) {
    CROS_CONFIG_LOG(ERROR) << prop_name << " phandle for model " << model_
                           << " is of size " << len << " but should be "
                           << sizeof(fdt32_t);
    return false;
  }
  int phandle = fdt32_to_cpu(*ptr);
  int offset = fdt_node_offset_by_phandle(blob, phandle);
  if (offset < 0) {
    CROS_CONFIG_LOG(ERROR) << prop_name << "lookup for model " << model_
                           << " failed: " << fdt_strerror(offset);
    return false;
  }
  *offset_out = offset;
  return true;
}

bool CrosConfig::InitCommon(const base::FilePath& filepath,
                            const base::FilePath& mem_file,
                            const base::FilePath& vpd_file) {
  // Many systems will not have a config database (yet), so just skip all the
  // setup without any errors if the config file doesn't exist.
  if (!base::PathExists(filepath)) {
    return false;
  }

  if (!base::ReadFileToString(filepath, &blob_)) {
    CROS_CONFIG_LOG(ERROR) << "Could not read file " << filepath.MaybeAsASCII();
    return false;
  }
  const void* blob = blob_.c_str();
  int ret = fdt_check_header(blob);
  if (ret) {
    CROS_CONFIG_LOG(ERROR) << "Config file " << filepath.MaybeAsASCII()
                           << " is invalid: " << fdt_strerror(ret);
    return false;
  }
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

  int target_dirs_offset = fdt_path_offset(blob, kTargetDirsPath);
  if (target_dirs_offset >= 0) {
    for (int poffset = fdt_first_property_offset(blob, target_dirs_offset);
         poffset >= 0;
         poffset = fdt_next_property_offset(blob, poffset)) {
      int len;
      const struct fdt_property* prop = fdt_get_property_by_offset(blob,
          poffset, &len);
      const char* name = fdt_string(blob, fdt32_to_cpu(prop->nameoff));
      target_dirs_[name] = prop->data;
    }
  } else if (target_dirs_offset < 0) {
    CROS_CONFIG_LOG(WARNING) << "Cannot find " << kTargetDirsPath
                             << " node: " << fdt_strerror(target_dirs_offset);
  }
  int schema_offset = fdt_path_offset(blob, kSchemaPath);
  if (schema_offset >= 0) {
    int len;
    const char* prop = static_cast<const char*>(
        fdt_getprop(blob, schema_offset, kPhandleProperties, &len));
    if (prop) {
      const char* end = prop + len;
      for (const char* ptr = prop; ptr < end; ptr += strlen(ptr) + 1) {
        phandle_props_.push_back(ptr);
      }
    } else {
      CROS_CONFIG_LOG(WARNING) << "Cannot find property " << kPhandleProperties
                               << " node: " << fdt_strerror(len);
    }
  } else if (schema_offset < 0) {
    CROS_CONFIG_LOG(WARNING) << "Cannot find " << kSchemaPath
                             << " node: " << fdt_strerror(schema_offset);
  }

  // See if there is a whitelabel config for this model.
  if (whitelabel_offset_ == -1) {
    LookupPhandle(model_offset_, "whitelabel", &whitelabel_offset_);
  }
  int next_offset;
  default_offsets_.clear();
  for (int offset = model_offset_;
       LookupPhandle(offset, "default", &next_offset);
       offset = next_offset) {
    if (std::find(default_offsets_.begin(), default_offsets_.end(),
                  next_offset) != default_offsets_.end()) {
      CROS_CONFIG_LOG(ERROR) << "Circular default at " << GetFullPath(offset);
      return false;
    }
    default_offsets_.push_back(next_offset);
  }

  CROS_CONFIG_LOG(INFO) << "Using master configuration for model "
                        << model_name_ << ", submodel "
                        << (submodel_name_.empty() ? "(none)" : submodel_name_);
  if (whitelabel_offset_ != -1) {
    CROS_CONFIG_LOG(INFO) << "Whitelabel of  "
                          << fdt_get_name(blob, whitelabel_offset_, NULL);
  } else if (whitelabel_tag_offset_ != -1) {
    CROS_CONFIG_LOG(INFO) << "Whitelabel tag "
                          << fdt_get_name(blob, whitelabel_tag_offset_, NULL);
  }
  inited_ = true;

  return true;
}

}  // namespace brillo
