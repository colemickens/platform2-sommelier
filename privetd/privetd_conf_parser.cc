// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/privetd_conf_parser.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/strings/string_utils.h>

namespace privetd {

namespace {

const char kWiFiBootstrapMode[] = "wifi_bootstrapping_mode";
const char kGcdBootstrapMode[] = "gcd_bootstrapping_mode";
const char kWiFiBootstrapInterfaces[] = "automatic_mode_interfaces";
const char kConnectTimeout[] = "connect_timeout_seconds";
const char kBootstrapTimeout[] = "bootstrap_timeout_seconds";
const char kMonitorTimeout[] = "monitor_timeout_seconds";

const char kBootstrapModeOff[] = "off";
const char kBootstrapModeAutomatic[] = "automatic";
const char kBootstrapModeManual[] = "manual";

}  // namespace

bool PrivetdConfigParser::Parse(const chromeos::KeyValueStore& config_store) {
  std::string wifi_bootstrap_mode_str;
  if (config_store.GetString(kWiFiBootstrapMode, &wifi_bootstrap_mode_str)) {
    if (wifi_bootstrap_mode_str.compare(kBootstrapModeOff) == 0) {
      wifi_bootstrap_mode_ = WiFiBootstrapMode::kDisabled;
    } else if (wifi_bootstrap_mode_str.compare(kBootstrapModeAutomatic) == 0) {
      wifi_bootstrap_mode_ = WiFiBootstrapMode::kAutomatic;
    } else if (wifi_bootstrap_mode_str.compare(kBootstrapModeManual) == 0) {
      LOG(ERROR) << "Manual WiFi bootstrapping mode is unsupported.";
      return false;
    } else {
      LOG(ERROR) << "Unrecognized WiFi bootstrapping mode: "
                 << wifi_bootstrap_mode_str;
      return false;
    }
  }

  std::string gcd_bootstrap_mode_str;
  if (config_store.GetString(kGcdBootstrapMode, &gcd_bootstrap_mode_str)) {
    if (gcd_bootstrap_mode_str.compare(kBootstrapModeOff) == 0) {
      gcd_bootstrap_mode_ = GcdBootstrapMode::kDisabled;
    } else if (gcd_bootstrap_mode_str.compare(kBootstrapModeAutomatic) == 0) {
      gcd_bootstrap_mode_ = GcdBootstrapMode::kAutomatic;
    } else if (gcd_bootstrap_mode_str.compare(kBootstrapModeManual) == 0) {
      LOG(ERROR) << "Manual GCD bootstrapping mode is unsupported.";
      return false;
    } else {
      LOG(ERROR) << "Unrecognized GCD bootstrapping mode: "
                 << gcd_bootstrap_mode_str;
      return false;
    }
  }

  std::string wifi_interface_list_str;
  if (config_store.GetString(kWiFiBootstrapInterfaces,
                             &wifi_interface_list_str)) {
    automatic_wifi_interfaces_ =
        chromeos::string_utils::Split(wifi_interface_list_str, ',', true, true);
  }

  auto parse_timeout = [](const std::string& input, uint32_t* parsed_value) {
    uint32_t temp{0};
    // Tragically, this will set |temp| even for invalid parsings.
    if (base::StringToUint(input, &temp)) {
      *parsed_value = temp;  // Parse worked, use that value.
      return true;
    }
    return false;
  };

  std::string connect_timeout_seconds_str;
  if (config_store.GetString(kConnectTimeout, &connect_timeout_seconds_str) &&
      !parse_timeout(connect_timeout_seconds_str, &connect_timeout_seconds_)) {
    LOG(ERROR) << "Invalid string given for connect timeout: "
               << connect_timeout_seconds_str;
    return false;
  }

  std::string bootstrap_timeout_seconds_str;
  if (config_store.GetString(kBootstrapTimeout,
                             &bootstrap_timeout_seconds_str) &&
      !parse_timeout(bootstrap_timeout_seconds_str,
                     &bootstrap_timeout_seconds_)) {
    LOG(ERROR) << "Invalid string given for bootstrap timeout: "
               << bootstrap_timeout_seconds_str;
    return false;
  }

  std::string monitor_timeout_seconds_str;
  if (config_store.GetString(kMonitorTimeout, &monitor_timeout_seconds_str) &&
      !parse_timeout(monitor_timeout_seconds_str, &monitor_timeout_seconds_)) {
    LOG(ERROR) << "Invalid string given for monitor timeout: "
               << monitor_timeout_seconds_str;
    return false;
  }

  return true;
}

}  // namespace privetd
