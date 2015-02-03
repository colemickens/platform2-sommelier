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
const char kDeviceServices[] = "device_services";
const char kDeviceClass[] = "device_class";
const char kDeviceMake[] = "device_make";
const char kDeviceModel[] = "device_model";
const char kDeviceModelId[] = "device_model_id";
const char kDeviceName[] = "device_name";
const char kDeviceDescription[] = "device_description";
const char kEmbeddedCode[] = "embedded_code";

const char kBootstrapModeOff[] = "off";
const char kBootstrapModeAutomatic[] = "automatic";
const char kBootstrapModeManual[] = "manual";

const char kDefaultDeviceClass[] = "AA";  // Generic device.
const char kDefaultDeviceMake[] = "Chromium";
const char kDefaultDeviceModel[] = "Brillo";
const char kDefaultDeviceModelId[] = "AAA";  // Model is not registered.

}  // namespace

PrivetdConfigParser::PrivetdConfigParser()
    : wifi_bootstrap_mode_{WiFiBootstrapMode::kDisabled},
      gcd_bootstrap_mode_{GcdBootstrapMode::kDisabled},
      connect_timeout_seconds_{60u},
      bootstrap_timeout_seconds_{600u},
      monitor_timeout_seconds_{120u},
      device_class_{kDefaultDeviceClass},
      device_make_{kDefaultDeviceMake},
      device_model_{kDefaultDeviceModel},
      device_model_id_{kDefaultDeviceModelId},
      device_name_{device_make_ + " " + device_model_} {
}

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

  std::string device_services_str;
  if (config_store.GetString(kDeviceServices, &device_services_str)) {
    device_services_ =
        chromeos::string_utils::Split(device_services_str, ',', true, true);
    for (const std::string& service : device_services_) {
      if (service.front() != '_') {
        LOG(ERROR) << "Invalid service name: " << service;
        return false;
      }
    }
  }

  config_store.GetString(kDeviceClass, &device_class_);
  if (device_class_.size() != 2) {
    LOG(ERROR) << "Invalid device class: " << device_class_;
    return false;
  }

  config_store.GetString(kDeviceMake, &device_make_);

  config_store.GetString(kDeviceModel, &device_model_);

  config_store.GetString(kDeviceModelId, &device_model_id_);
  if (device_model_id_.size() != 3) {
    LOG(ERROR) << "Invalid model id: " << device_model_id_;
    return false;
  }

  config_store.GetString(kDeviceName, &device_name_);
  if (device_name_.empty()) {
    LOG(ERROR) << "Empty device name";
    return false;
  }

  config_store.GetString(kDeviceDescription, &device_description_);

  config_store.GetString(kEmbeddedCode, &embedded_code_);

  return true;
}

}  // namespace privetd
