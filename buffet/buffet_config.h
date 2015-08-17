// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_BUFFET_CONFIG_H_
#define BUFFET_BUFFET_CONFIG_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <chromeos/key_value_store.h>
#include <weave/config_store.h>

namespace buffet {

class StorageInterface;

struct BuffetConfigPaths {
  base::FilePath defaults;
  base::FilePath settings;

  base::FilePath definitions;
  base::FilePath test_definitions;
};

// Handles reading buffet config and state files.
class BuffetConfig final : public weave::ConfigStore {
 public:
  using OnChangedCallback = base::Callback<void(const weave::Settings&)>;
  ~BuffetConfig() override = default;

  explicit BuffetConfig(const BuffetConfigPaths& paths);

  // Config overrides.
  bool LoadDefaults(weave::Settings* settings) override;
  std::string LoadSettings() override;
  void SaveSettings(const std::string& settings) override;
  void OnSettingsChanged(const weave::Settings& settings) override;
  std::string LoadBaseCommandDefs() override;
  std::map<std::string, std::string> LoadCommandDefs() override;
  std::string LoadBaseStateDefs() override;
  std::string LoadBaseStateDefaults() override;
  std::map<std::string, std::string> LoadStateDefs() override;
  std::vector<std::string> LoadStateDefaults() override;

  void AddOnChangedCallback(const OnChangedCallback& callback);

  bool LoadDefaults(const chromeos::KeyValueStore& store,
                    weave::Settings* settings);

 private:
  BuffetConfigPaths paths_;

  std::vector<OnChangedCallback> on_changed_;

  DISALLOW_COPY_AND_ASSIGN(BuffetConfig);
};

}  // namespace buffet

#endif  // BUFFET_BUFFET_CONFIG_H_
