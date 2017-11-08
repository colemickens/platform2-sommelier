// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"

// TODO(sjg@chromium.org): See if this can be accepted upstream.
extern "C" {
#include <libfdt.h>
};

#include <iostream>
#include <sstream>
#include <string>

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/process/launch.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>

namespace {
const char kConfigDtbPath[] = "/usr/share/chromeos-config/config.dtb";
const char kTargetDirsPath[] = "/chromeos/schema/target-dirs";
}  // namespace

namespace brillo {

CrosConfig::CrosConfig() {}

CrosConfig::~CrosConfig() {}

bool CrosConfig::Init() {
  return InitModel();
}

bool CrosConfig::InitModel() {
  const base::FilePath::CharType* const argv[] = {
      "mosys", "-k", "platform", "id"};
  base::CommandLine cmdline(arraysize(argv), argv);

  return InitCommon(base::FilePath(kConfigDtbPath), cmdline);
}

bool CrosConfig::InitForTest(const base::FilePath& filepath,
                             const std::string& name, int sku_id,
                             const std::string& whitelabel_tag) {
  std::string output = base::StringPrintf(
      "name=\"%s\"\\nsku=\"%d\"\ncustomization=\"%s\"",
      name.c_str(), sku_id, whitelabel_tag.c_str());
  const base::FilePath::CharType* const argv[] = {"echo", "-e", output.c_str()};
  base::CommandLine cmdline(arraysize(argv), argv);

  return InitCommon(filepath, cmdline);
}

std::string CrosConfig::GetFullPath(int offset) {
  const void* blob = blob_.c_str();
  char buf[256];
  int err;

  err = fdt_get_path(blob, offset, buf, sizeof(buf));
  if (err) {
    LOG(WARNING) << "Cannot get full path: " << fdt_strerror(err);
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
                           const std::string& prop, std::string* val_out) {
  const void* blob = blob_.c_str();

  int subnode = GetPathOffset(base_offset, path);
  int wl_subnode = -1;
  if (whitelabel_offset_ != -1) {
    wl_subnode = GetPathOffset(whitelabel_offset_, path);
    if (subnode < 0 && wl_subnode >= 0) {
      LOG(INFO) << "The path " << GetFullPath(base_offset) << path
                << " does not exist. Falling back to whitelabel path";
      subnode = wl_subnode;
    }
  }
  if (subnode < 0) {
    LOG(ERROR) << "The path " << GetFullPath(base_offset) << path
               << " does not exist.";
    return false;
  }

  int len = 0;
  const char* ptr = static_cast<const char*>(
      fdt_getprop(blob, subnode, prop.c_str(), &len));
  if (!ptr && wl_subnode >= 0) {
    ptr = static_cast<const char*>(fdt_getprop(blob, wl_subnode, prop.c_str(),
                                               &len));
    LOG(INFO) << "The property " << prop << " does not exist. Falling back to "
              << "whitelabel property";
  }
  // We would prefer to do this lookup on the host where the full schema info
  // is available. But at present this is not implemented. We want this for
  // audio, so add a check there.
  // Perhaps we can resolve this as part of crbug.com/761284
  if (!ptr) {
    int target_node;
    LookupPhandle(subnode, "audio-type", &target_node);
    if (target_node >= 0) {
      ptr = static_cast<const char*>(
        fdt_getprop(blob, target_node, prop.c_str(), &len));
      if (ptr) {
        LOG(INFO) << "Followed audio-type phandle";
      }
    }
  }

  if (!ptr || len < 0) {
    LOG(WARNING) << "Cannot get path " << path << " property " << prop << ": "
                 << "full path " << GetFullPath(subnode) << ": "
                 << fdt_strerror(len);
    return false;
  }

  // We must have a normally terminated string. This guards against a string
  // list being used, or perhaps a property that does not contain a valid
  // string at all.
  if (!len || strnlen(ptr, len) != static_cast<size_t>(len - 1)) {
    LOG(ERROR) << "String at path " << path << " property " << prop
               << " is invalid";
    return false;
  }

  val_out->assign(ptr);

  return true;
}

bool CrosConfig::GetString(const std::string& path, const std::string& prop,
                           std::string* val_out) {
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

  if (whitelabel_tag_offset_ != -1) {
    if (path == "/" && GetString(whitelabel_tag_offset_, "/", prop, val_out)) {
      return true;
    }
    // TODO(sjg@chromium.org): We are considering moving the key-id to the root
    // of the model schema. If we do, we can drop this special case.
    if (path == "/firmware" && prop == "key-id" &&
        GetString(whitelabel_tag_offset_, "/", prop, val_out)) {
      return true;
    }
  }
  if (!GetString(model_offset_, path, prop, val_out)) {
    if (submodel_offset_ != -1 &&
        GetString(submodel_offset_, path, prop, val_out)) {
      return true;
    }
    if (default_offset_ != -1) {
      return GetString(default_offset_, path, prop, val_out);
    }
    return false;
  }
  return true;
}

bool CrosConfig::GetAbsPath(const std::string& path, const std::string& prop,
                            std::string* val_out) {
  const void* blob = blob_.c_str();
  std::string val;
  if (!GetString(path, prop, &val)) {
    return false;
  }

  if (target_dirs_offset_ == -1) {
    LOG(ERROR) << "Absolute path requested at path " << path << " property "
               << prop  << " but no target-dirs are available";
    return false;
  }
  int len;
  const char* ptr = static_cast<const char*>(
      fdt_getprop(blob, target_dirs_offset_, prop.c_str(), &len));

  if (!ptr) {
    LOG(ERROR) << "Absolute path requested at path " << path << " property "
               << prop  << ": " << fdt_strerror(len);
    return false;
  }
  *val_out = std::string(ptr) + "/" + val;

  return true;
}

bool CrosConfig::LookupPhandle(int node_offset, const std::string &prop_name,
                               int *offset_out) {
  const void* blob = blob_.c_str();
  int len;
  const fdt32_t* ptr = static_cast<const fdt32_t*>(
      fdt_getprop(blob, node_offset, prop_name.c_str(), &len));

  // We probably don't need all these checks since validation will ensure that
  // the config is correct. But this is a critical tool and we want to avoid
  // crashes in any situation.
  *offset_out = -1;
  if (ptr) {
    if (len != sizeof(fdt32_t)) {
      LOG(ERROR) << prop_name << " phandle for model " << model_
                  << " is of size " << len << " but should be "
                  << sizeof(fdt32_t);
      return false;
    }
    int phandle = fdt32_to_cpu(*ptr);
    int offset = fdt_node_offset_by_phandle(blob, phandle);
    if (offset < 0) {
      LOG(ERROR) << prop_name << "lookup for model " << model_ << " failed: "
                  << fdt_strerror(offset);
      return false;
    }
    *offset_out = offset;
  }
  return true;
}

bool CrosConfig::DecodeIdentifiers(const std::string &output,
                                   std::string* name_out, int* sku_id_out,
                                   std::string* whitelabel_tag_out) {
  *sku_id_out = -1;
  std::istringstream ss(output);
  std::string line;
  base::StringPairs pairs;
  if (!base::SplitStringIntoKeyValuePairs(output, '=', '\n', &pairs)) {
    LOG(ERROR) << "Cannot decode mosys output " << output;
    return false;
  }
  for (const auto &pair : pairs) {
    if (pair.second.length() < 2) {
      LOG(ERROR) << "Cannot decode mosys value " << pair.second;
      return false;
    }
    std::string value = pair.second.substr(1, pair.second.length() - 2);
    if (pair.first == "name") {
      *name_out = value;
    } else if (pair.first == "sku") {
      *sku_id_out = std::stoi(value);
    } else if (pair.first == "customization") {
      *whitelabel_tag_out = value;
    }
  }
  return true;
}

bool CrosConfig::InitCommon(const base::FilePath& filepath,
                            const base::CommandLine& cmdline) {
  // Many systems will not have a config database (yet), so just skip all the
  // setup without any errors if the config file doesn't exist.
  if (!base::PathExists(filepath)) {
    return false;
  }

  if (!base::ReadFileToString(filepath, &blob_)) {
    LOG(ERROR) << "Could not read file " << filepath.MaybeAsASCII();
    return false;
  }

  std::string output;
  if (!base::GetAppOutput(cmdline, &output)) {
    LOG(ERROR) << "Could not run command " << cmdline.GetCommandLineString();
    return false;
  }
  std::string name;
  int sku_id;
  std::string whitelabel_tag;
  if (!DecodeIdentifiers(output, &name, &sku_id, &whitelabel_tag)) {
    LOG(ERROR) << "Could not decode output " << output;
    return false;
  }
  const void* blob = blob_.c_str();
  int ret = fdt_check_header(blob);
  if (ret) {
    LOG(ERROR) << "Config file " << filepath.MaybeAsASCII() << " is invalid: "
               << fdt_strerror(ret);
    return false;
  }
  if (!SelectModelConfigByIDs(name, sku_id, whitelabel_tag)) {
    LOG(ERROR) << "Cannot find SKU for name " << name << " SKU ID " << sku_id;
    return false;
  }

  int target_dirs_offset = fdt_path_offset(blob, kTargetDirsPath);
  if (target_dirs_offset >= 0) {
    target_dirs_offset_ = target_dirs_offset;
  } else if (target_dirs_offset < 0) {
    LOG(WARNING) << "Cannot find " << kTargetDirsPath << " node: "
               << fdt_strerror(target_dirs_offset);
  }
  // See if there is a whitelabel config for this model.
  if (whitelabel_offset_ == -1) {
    LookupPhandle(model_offset_, "whitelabel", &whitelabel_offset_);
  }
  LookupPhandle(model_offset_, "default", &default_offset_);

  LOG(INFO) << "Using master configuration for model " << model_name_
            << ", submodel "
            << (submodel_name_.empty() ? "(none)" : submodel_name_);
  if (whitelabel_offset_ != -1) {
    LOG(INFO) << "Whiltelael of  "
              << fdt_get_name(blob, whitelabel_offset_, NULL);
  } else if (whitelabel_tag_offset_ != -1) {
    LOG(INFO) << "Whiltelael tag "
              << fdt_get_name(blob, whitelabel_tag_offset_, NULL);
  }
  inited_ = true;

  return true;
}

bool CrosConfig::InitCheck() const {
  if (!inited_) {
    LOG(ERROR) << "Init*() must be called before accessing configuration";
    return false;
  }
  return true;
}

}  // namespace brillo
