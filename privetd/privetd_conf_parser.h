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

bool ParseConfigFile(const chromeos::KeyValueStore& config_store,
                     WiFiBootstrapMode* wifi_bootstrap_mode,
                     GcdBootstrapMode* gcd_bootstrap_mode,
                     std::vector<std::string>* automatic_wifi_interfaces,
                     uint32_t* connect_timeout_seconds,
                     uint32_t* bootstrap_timeout_seconds,
                     uint32_t* monitor_timeout_seconds);

}  // namespace privetd

#endif  // PRIVETD_PRIVETD_CONF_PARSER_H_
