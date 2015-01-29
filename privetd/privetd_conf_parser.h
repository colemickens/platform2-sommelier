// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_PRIVETD_CONF_PARSER_H_
#define PRIVETD_PRIVETD_CONF_PARSER_H_

#include <string>
#include <vector>

#include <chromeos/key_value_store.h>

namespace privetd {

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

namespace config_key {

extern const char kWiFiBootstrapMode[];
extern const char kGcdBootstrapMode[];
extern const char kWiFiBootstrapInterfaces[];
extern const char kConnectTimeout[];
extern const char kBootstrapTimeout[];
extern const char kMonitorTimeout[];

}  // namespace config_key

class PrivetdConfigParser {
 public:
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

 private:
  WiFiBootstrapMode wifi_bootstrap_mode_{WiFiBootstrapMode::kDisabled};
  GcdBootstrapMode gcd_bootstrap_mode_{GcdBootstrapMode::kDisabled};
  std::vector<std::string> automatic_wifi_interfaces_;
  uint32_t connect_timeout_seconds_{60u};
  uint32_t bootstrap_timeout_seconds_{600u};
  uint32_t monitor_timeout_seconds_{120u};
};

}  // namespace privetd

#endif  // PRIVETD_PRIVETD_CONF_PARSER_H_
