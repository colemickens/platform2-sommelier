// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Library to provide access to the Chrome OS master configuration

#include "chromeos-config/libcros_config/cros_config.h"

#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/process/launch.h>
#include <base/strings/string_split.h>

namespace {
const char kMosysPlatformIdPath[] = "/run/mosys/platform_id";
}  // namespace

namespace brillo {

CrosConfig::CrosConfig() {}

CrosConfig::~CrosConfig() {}

bool CrosConfig::Init() {
  return InitModel();
}

bool CrosConfig::InitForConfig(const base::FilePath& filepath) {
  const base::FilePath::CharType* const argv[] = {
      "mosys", "-k", "platform", "id"};
  base::CommandLine cmdline(arraysize(argv), argv);

  std::string platform_id_output;
  if (!base::ReadFileToString(base::FilePath(kMosysPlatformIdPath),
                              &platform_id_output)) {
    LOG(WARNING) << "Could not read cache from " << kMosysPlatformIdPath
                 << "; calling mosys...";
    if (!base::GetAppOutput(cmdline, &platform_id_output)) {
      LOG(ERROR) << "Could not run command " << cmdline.GetCommandLineString();
      return false;
    }
  }

  std::string name;
  int sku_id;
  std::string whitelabel_name;
  if (!DecodeIdentifiers(platform_id_output, &name, &sku_id,
                         &whitelabel_name)) {
    LOG(ERROR) << "Could not decode platform id " << platform_id_output;
    return false;
  }

  return InitCommon(filepath, name, sku_id,
                    whitelabel_name);
}

bool CrosConfig::InitForTest(const base::FilePath& filepath,
                             const std::string& name, int sku_id,
                             const std::string& whitelabel_name) {
  return InitCommon(filepath, name, sku_id, whitelabel_name);
}

bool CrosConfig::InitCheck() const {
  if (!inited_) {
    LOG(ERROR) << "Init*() must be called before accessing configuration";
    return false;
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

}  // namespace brillo
