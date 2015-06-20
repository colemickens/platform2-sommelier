// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/privet/privetd_conf_parser.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/strings/string_utils.h>

#include "buffet/privet/security_delegate.h"

namespace privetd {

namespace {

const char kWiFiBootstrapMode[] = "wifi_bootstrapping_mode";
const char kPairingModes[] = "pairing_modes";
const char kEmbeddedCodePath[] = "embedded_code_path";

const char kBootstrapModeOff[] = "off";

}  // namespace

PrivetdConfigParser::PrivetdConfigParser()
    : wifi_auto_setup_enabled_{true}, pairing_modes_{PairingType::kPinCode} {}

bool PrivetdConfigParser::Parse(const chromeos::KeyValueStore& config_store) {
  std::string wifi_bootstrap_mode_str;
  // TODO(vitalybuka): switch to boolean.
  if (config_store.GetString(kWiFiBootstrapMode, &wifi_bootstrap_mode_str)) {
    wifi_auto_setup_enabled_ = (wifi_bootstrap_mode_str != kBootstrapModeOff);
  }

  std::set<PairingType> pairing_modes;
  std::string embedded_code_path;
  if (config_store.GetString(kEmbeddedCodePath, &embedded_code_path)) {
    embedded_code_path_ = base::FilePath(embedded_code_path);
    if (!embedded_code_path_.empty())
      pairing_modes.insert(PairingType::kEmbeddedCode);
  }

  std::string modes_str;
  if (config_store.GetString(kPairingModes, &modes_str)) {
    for (const std::string& mode :
         chromeos::string_utils::Split(modes_str, ",", true, true)) {
      PairingType pairing_mode;
      if (!StringToPairingType(mode, &pairing_mode)) {
        LOG(ERROR) << "Invalid pairing mode : " << mode;
        return false;
      }
      pairing_modes.insert(pairing_mode);
    }
  }

  if (!pairing_modes.empty())
    pairing_modes_ = std::move(pairing_modes);

  return true;
}

}  // namespace privetd
