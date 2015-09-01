// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/examples/ubuntu/file_config_store.h"

#include <sys/stat.h>
#include <sys/utsname.h>

#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace weave {
namespace examples {

const char kSettingsDir[] = "/var/lib/weave/";
const char kSettingsPath[] = "/var/lib/weave/weave_settings.json";

bool FileConfigStore::LoadDefaults(Settings* settings) {
  char host_name[HOST_NAME_MAX] = {};
  gethostname(host_name, HOST_NAME_MAX);

  settings->name = host_name;
  settings->description = "";

  utsname uname_data;
  uname(&uname_data);

  settings->firmware_version = uname_data.sysname;
  settings->oem_name = "Unknown";
  settings->model_name = "Unknown";
  settings->model_id = "AAAAA";
  settings->pairing_modes = {PairingType::kEmbeddedCode};
  settings->embedded_code = "0000";
  return true;
}

std::string FileConfigStore::LoadSettings() {
  LOG(INFO) << "Loading settings from " << kSettingsPath;
  std::ifstream str(kSettingsPath);
  return std::string(std::istreambuf_iterator<char>(str),
                     std::istreambuf_iterator<char>());
}

void FileConfigStore::SaveSettings(const std::string& settings) {
  CHECK(mkdir(kSettingsDir, S_IRWXU) == 0 || errno == EEXIST);
  LOG(INFO) << "Saving settings to " << kSettingsPath;
  std::ofstream str(kSettingsPath);
  str << settings;
}

void FileConfigStore::OnSettingsChanged(const Settings& settings) {
  LOG(INFO) << "OnSettingsChanged";
}

std::string FileConfigStore::LoadBaseCommandDefs() {
  return R"({
    "base": {
      "updateBaseConfiguration": {
        "minimalRole": "manager",
        "parameters": {
          "localDiscoveryEnabled": "boolean",
          "localAnonymousAccessMaxRole": [ "none", "viewer", "user" ],
          "localPairingEnabled": "boolean"
        },
        "results": {}
      },
      "identify": {
        "minimalRole": "user",
        "parameters": {},
        "results": {}
      },
      "updateDeviceInfo": {
        "minimalRole": "manager",
        "parameters": {
          "description": "string",
          "name": "string",
          "location": "string"
        },
        "results": {}
      }
    }
  })";
}

std::map<std::string, std::string> FileConfigStore::LoadCommandDefs() {
  return {{"example", R"({
    "base": {
      "updateBaseConfiguration": {},
      "identify": {},
      "updateDeviceInfo": {}
    }
  })"}};
}

std::string FileConfigStore::LoadBaseStateDefs() {
  return R"({
    "base": {
      "firmwareVersion": "string",
      "localDiscoveryEnabled": "boolean",
      "localAnonymousAccessMaxRole": [ "none", "viewer", "user" ],
      "localPairingEnabled": "boolean",
      "network": {
        "properties": {
          "name": "string"
        }
      }
    }
  })";
}

std::string FileConfigStore::LoadBaseStateDefaults() {
  return R"({
    "base": {
      "firmwareVersion": "unknown",
      "localDiscoveryEnabled": false,
      "localAnonymousAccessMaxRole": "none",
      "localPairingEnabled": false
    }
  })";
}

std::map<std::string, std::string> FileConfigStore::LoadStateDefs() {
  return {};
}

std::vector<std::string> FileConfigStore::LoadStateDefaults() {
  return {};
}

}  // namespace examples
}  // namespace weave
