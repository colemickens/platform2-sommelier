// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_BUFFET_CONFIG_H_
#define BUFFET_BUFFET_CONFIG_H_

#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <chromeos/key_value_store.h>
#include <weave/config_store.h>

namespace buffet {

class StorageInterface;

// Handles reading buffet config and state files.
class BuffetConfig final : public weave::ConfigStore {
 public:
  using OnChangedCallback = base::Callback<void(const weave::Settings&)>;
  ~BuffetConfig() override = default;

  BuffetConfig(const base::FilePath& defaults_path,
               const base::FilePath& settings_path);

  // Config overrides.
  bool LoadDefaults(weave::Settings* settings) override;
  std::string LoadSettings() override;
  void SaveSettings(const std::string& settings) override;
  void OnSettingsChanged(const weave::Settings& settings) override;

  void AddOnChangedCallback(const OnChangedCallback& callback);

  bool LoadDefaults(const chromeos::KeyValueStore& store,
                    weave::Settings* settings);

 private:
  base::FilePath defaults_path_;
  base::FilePath settings_path_;

  std::vector<OnChangedCallback> on_changed_;

  DISALLOW_COPY_AND_ASSIGN(BuffetConfig);
};

}  // namespace buffet

#endif  // BUFFET_BUFFET_CONFIG_H_
