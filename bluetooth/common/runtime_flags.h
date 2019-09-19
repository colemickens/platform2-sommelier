// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_COMMON_RUNTIME_FLAGS_H_
#define BLUETOOTH_COMMON_RUNTIME_FLAGS_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <chromeos-config/libcros_config/cros_config.h>

namespace bluetooth {

// Path to bluetooth flags in chromeos-config
constexpr char kBluetoothPath[] = "/bluetooth/flags";

class RuntimeFlags {
 public:
  RuntimeFlags() = default;
  ~RuntimeFlags() = default;

  // Prepare flags for access
  void Init();

  // Get the flag setting.
  // Returns: True if key exists and is truthy (1, true, True)
  bool Get(const std::string& key);

  // If the flag has string content get the value.
  // Returns: True if key exists.
  bool GetContent(const std::string& key, std::string* out);

 private:
  const std::map<std::string, bool>* use_flags_;
  std::unique_ptr<brillo::CrosConfig> cros_config_;
  bool init_ = false;

  DISALLOW_COPY_AND_ASSIGN(RuntimeFlags);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_COMMON_RUNTIME_FLAGS_H_
