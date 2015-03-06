// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_PRIVETD_CONF_PARSER_H_
#define PRIVETD_PRIVETD_CONF_PARSER_H_

#include <string>
#include <vector>

#include <chromeos/key_value_store.h>

namespace privetd {

extern const char kWiFiBootstrapInterfaces[];

enum class WiFiBootstrapMode {
  kDisabled,
  kManual,
  kAutomatic,
};

enum class GcdBootstrapMode {
  kDisabled,
  kManual,
  kAutomatic,
};

enum class PairingType;

class PrivetdConfigParser {
 public:
  PrivetdConfigParser();

  bool Parse(const chromeos::KeyValueStore& config_store);

  WiFiBootstrapMode wifi_bootstrap_mode() const { return wifi_bootstrap_mode_; }
  GcdBootstrapMode gcd_bootstrap_mode() const { return gcd_bootstrap_mode_; }
  const std::vector<std::string>& automatic_wifi_interfaces() const {
    return automatic_wifi_interfaces_;
  }
  uint32_t connect_timeout_seconds() const { return connect_timeout_seconds_; }
  uint32_t bootstrap_timeout_seconds() const {
    return bootstrap_timeout_seconds_;
  }
  uint32_t monitor_timeout_seconds() const { return monitor_timeout_seconds_; }
  const std::vector<std::string>& device_services() const {
    return device_services_;
  }
  const std::string& device_class() const { return device_class_; }
  const std::string& device_make() const { return device_make_; }
  const std::string& device_model() const { return device_model_; }
  const std::string& device_model_id() const { return device_model_id_; }
  const std::string& device_name() const { return device_name_; }
  const std::string& device_description() const { return device_description_; }
  const std::vector<PairingType>& pairing_modes() { return pairing_modes_; }
  const base::FilePath& embedded_code_path() const {
    return embedded_code_path_;
  }

 private:
  WiFiBootstrapMode wifi_bootstrap_mode_;
  GcdBootstrapMode gcd_bootstrap_mode_;
  std::vector<std::string> automatic_wifi_interfaces_;
  uint32_t connect_timeout_seconds_;
  uint32_t bootstrap_timeout_seconds_;
  uint32_t monitor_timeout_seconds_;
  std::vector<std::string> device_services_;
  std::string device_class_;
  std::string device_make_;
  std::string device_model_;
  std::string device_model_id_;
  std::string device_name_;
  std::string device_description_;
  std::vector<PairingType> pairing_modes_;
  base::FilePath embedded_code_path_;
};

}  // namespace privetd

#endif  // PRIVETD_PRIVETD_CONF_PARSER_H_
