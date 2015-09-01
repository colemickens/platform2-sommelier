// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_EXAMPLES_UBUNTU_FILE_CONFIG_STORE_H_
#define LIBWEAVE_EXAMPLES_UBUNTU_FILE_CONFIG_STORE_H_

#include <map>
#include <string>
#include <vector>

#include <weave/config_store.h>

namespace weave {
namespace examples {

class FileConfigStore : public ConfigStore {
 public:
  bool LoadDefaults(Settings* settings) override;
  std::string LoadSettings() override;
  void SaveSettings(const std::string& settings) override;
  void OnSettingsChanged(const Settings& settings) override;
  std::string LoadBaseCommandDefs() override;
  std::map<std::string, std::string> LoadCommandDefs() override;
  std::string LoadBaseStateDefs() override;
  std::string LoadBaseStateDefaults() override;
  std::map<std::string, std::string> LoadStateDefs() override;
  std::vector<std::string> LoadStateDefaults() override;
};

}  // namespace examples
}  // namespace weave

#endif  // LIBWEAVE_EXAMPLES_UBUNTU_FILE_CONFIG_STORE_H_
