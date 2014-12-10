// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/config.h"

#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "apmanager/device.h"
#include "apmanager/manager.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;
using chromeos::ErrorPtr;
using std::string;

namespace apmanager {

// static
const char Config::kHostapdConfigKeyBridgeInterface[] = "bridge";
const char Config::kHostapdConfigKeyChannel[] = "channel";
const char Config::kHostapdConfigKeyControlInterface[] = "ctrl_interface";
const char Config::kHostapdConfigKeyDriver[] = "driver";
const char Config::kHostapdConfigKeyFragmThreshold[] = "fragm_threshold";
const char Config::kHostapdConfigKeyHTCapability[] = "ht_capab";
const char Config::kHostapdConfigKeyHwMode[] = "hw_mode";
const char Config::kHostapdConfigKeyIeee80211ac[] = "ieee80211ac";
const char Config::kHostapdConfigKeyIeee80211n[] = "ieee80211n";
const char Config::kHostapdConfigKeyIgnoreBroadcastSsid[] =
    "ignore_broadcast_ssid";
const char Config::kHostapdConfigKeyInterface[] = "interface";
const char Config::kHostapdConfigKeyRsnPairwise[] = "rsn_pairwise";
const char Config::kHostapdConfigKeyRtsThreshold[] = "rts_threshold";
const char Config::kHostapdConfigKeySsid[] = "ssid";
const char Config::kHostapdConfigKeyWepDefaultKey[] = "wep_default_key";
const char Config::kHostapdConfigKeyWepKey0[] = "wep_key0";
const char Config::kHostapdConfigKeyWpa[] = "wpa";
const char Config::kHostapdConfigKeyWpaKeyMgmt[] = "wpa_key_mgmt";
const char Config::kHostapdConfigKeyWpaPassphrase[] = "wpa_passphrase";

const char Config::kHostapdHwMode80211a[] = "a";
const char Config::kHostapdHwMode80211b[] = "b";
const char Config::kHostapdHwMode80211g[] = "g";

// static
const uint16_t Config::kPropertyDefaultChannel = 6;
const uint16_t Config::kPropertyDefaultServerAddressIndex = 0;
const bool Config::kPropertyDefaultHiddenNetwork = false;

// static
const char Config::kHostapdDefaultDriver[] = "nl80211";
const char Config::kHostapdDefaultRsnPairwise[] = "CCMP";
const char Config::kHostapdDefaultWpaKeyMgmt[] = "WPA-PSK";
// Fragmentation threshold: disabled.
const int Config::kHostapdDefaultFragmThreshold = 2346;
// RTS threshold: disabled.
const int Config::kHostapdDefaultRtsThreshold = 2347;

// static
const uint16_t Config::kBand24GHzChannelLow = 1;
const uint16_t Config::kBand24GHzChannelHigh = 13;
const uint32_t Config::kBand24GHzBaseFrequency = 2412;
const uint16_t Config::kBand5GHzChannelLow = 34;
const uint16_t Config::kBand5GHzChannelHigh = 165;
const uint16_t Config::kBand5GHzBaseFrequency = 5170;

// static
const int Config::kSsidMinLength = 1;
const int Config::kSsidMaxLength = 32;
const int Config::kPassphraseMinLength = 8;
const int Config::kPassphraseMaxLength = 63;

Config::Config(Manager* manager, const string& service_path)
    : org::chromium::apmanager::ConfigAdaptor(this),
      manager_(manager),
      dbus_path_(dbus::ObjectPath(
          base::StringPrintf("%s/config", service_path.c_str()))) {
  // Initialize default configuration values.
  SetSecurityMode(kSecurityModeNone);
  SetHwMode(kHwMode80211g);
  SetOperationMode(kOperationModeServer);
  SetServerAddressIndex(kPropertyDefaultServerAddressIndex);
  SetChannel(kPropertyDefaultChannel);
  SetHiddenNetwork(kPropertyDefaultHiddenNetwork);
}

Config::~Config() {}

// static.
bool Config::GetFrequencyFromChannel(uint16_t channel, uint32_t* freq) {
  bool ret_value = true;
  if (channel >= kBand24GHzChannelLow && channel <= kBand24GHzChannelHigh) {
    *freq = kBand24GHzBaseFrequency + (channel - kBand24GHzChannelLow) * 5;
  } else if (channel >= kBand5GHzChannelLow &&
             channel <= kBand5GHzChannelHigh) {
    *freq = kBand5GHzBaseFrequency + (channel - kBand5GHzChannelLow) * 5;
  } else {
    ret_value = false;
  }
  return ret_value;
}

bool Config::ValidateSsid(ErrorPtr* error, const string& value) {
  if (value.length() < kSsidMinLength || value.length() > kSsidMaxLength) {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
        "SSID must contain between %d and %d characters",
        kSsidMinLength, kSsidMaxLength);
    return false;
  }
  return true;
}

bool Config::ValidateSecurityMode(ErrorPtr* error, const string& value) {
  if (value != kSecurityModeNone && value != kSecurityModeRSN) {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
        "Invalid/unsupported security mode [%s]", value.c_str());
    return false;
  }
  return true;
}

bool Config::ValidatePassphrase(ErrorPtr* error, const string& value) {
  if (value.length() < kPassphraseMinLength ||
      value.length() > kPassphraseMaxLength) {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
        "Passphrase must contain between %d and %d characters",
        kPassphraseMinLength, kPassphraseMaxLength);
    return false;
  }
  return true;
}

bool Config::ValidateHwMode(ErrorPtr* error, const string& value) {
  if (value != kHwMode80211a && value != kHwMode80211b &&
      value != kHwMode80211g && value != kHwMode80211n &&
      value != kHwMode80211ac) {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
        "Invalid HW mode [%s]", value.c_str());
    return false;
  }
  return true;
}

bool Config::ValidateOperationMode(ErrorPtr* error, const string& value) {
  if (value != kOperationModeServer && value != kOperationModeBridge) {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
        "Invalid operation mode [%s]", value.c_str());
    return false;
  }
  return true;
}

bool Config::ValidateChannel(ErrorPtr* error, const uint16_t& value) {
  if ((value >= kBand24GHzChannelLow && value <= kBand24GHzChannelHigh) ||
      (value >= kBand5GHzChannelLow && value <= kBand5GHzChannelHigh)) {
    return true;
  }
  chromeos::Error::AddToPrintf(
      error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
      "Invalid channel [%d]", value);
  return false;
}

void Config::RegisterAsync(ExportedObjectManager* object_manager,
                           const scoped_refptr<dbus::Bus>& bus,
                           AsyncEventSequencer* sequencer) {
  CHECK(!dbus_object_) << "Already registered";
  dbus_object_.reset(
      new chromeos::dbus_utils::DBusObject(
          object_manager,
          bus,
          dbus_path_));
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Config.RegisterAsync() failed.", true));
}

bool Config::GenerateConfigFile(ErrorPtr* error, string* config_str) {
  // SSID.
  string ssid = GetSsid();
  if (ssid.empty()) {
    chromeos::Error::AddTo(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
        "SSID not specified");
    return false;
  }
  base::StringAppendF(
      config_str, "%s=%s\n", kHostapdConfigKeySsid, ssid.c_str());

  // Bridge interface is required for bridge mode operation.
  if (GetOperationMode() == kOperationModeBridge) {
    if (GetBridgeInterface().empty()) {
      chromeos::Error::AddTo(
          error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
          "Bridge interface not specified, required for bridge mode");
      return false;
    }
    base::StringAppendF(config_str,
                        "%s=%s\n",
                        kHostapdConfigKeyBridgeInterface,
                        GetBridgeInterface().c_str());
  }

  // Channel.
  base::StringAppendF(
      config_str, "%s=%d\n", kHostapdConfigKeyChannel, GetChannel());

  // Interface.
  if (!AppendInterface(error, config_str)) {
    return false;
  }

  // Hardware mode.
  if (!AppendHwMode(error, config_str)) {
    return false;
  }

  // Security mode configurations.
  if (!AppendSecurityMode(error, config_str)) {
    return false;
  }

  // Hostapd default configurations.
  if (!AppendHostapdDefaults(error, config_str)) {
    return false;
  }

  return true;
}

bool Config::ClaimDevice() {
  if (!device_) {
    LOG(ERROR) << "Failed to claim device: device doesn't exist.";
    return false;
  }
  return device_->ClaimDevice();
}

bool Config::ReleaseDevice() {
  if (!device_) {
    LOG(ERROR) << "Failed to release device: device doesn't exist.";
    return false;
  }
  return device_->ReleaseDevice();
}

bool Config::AppendHwMode(ErrorPtr* error, std::string* config_str) {
  string hw_mode = GetHwMode();
  string hostapd_hw_mode;
  if (hw_mode == kHwMode80211a) {
    hostapd_hw_mode = kHostapdHwMode80211a;
  } else if (hw_mode == kHwMode80211b) {
    hostapd_hw_mode = kHostapdHwMode80211b;
  } else if (hw_mode == kHwMode80211g) {
    hostapd_hw_mode = kHostapdHwMode80211g;
  } else if (hw_mode == kHwMode80211n) {
    // Use 802.11a for 5GHz channel and 802.11g for 2.4GHz channel
    if (GetChannel() >= 34) {
      hostapd_hw_mode = kHostapdHwMode80211a;
    } else {
      hostapd_hw_mode = kHostapdHwMode80211g;
    }
    base::StringAppendF(config_str, "%s=1\n", kHostapdConfigKeyIeee80211n);

    // Get HT Capability.
    string ht_cap;
    if (!device_->GetHTCapability(GetChannel(), &ht_cap)) {
      chromeos::Error::AddTo(
          error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
          "Failed to get HT Capability");
      return false;
    }
    base::StringAppendF(config_str, "%s=%s\n",
                        kHostapdConfigKeyHTCapability,
                        ht_cap.c_str());
  } else if (hw_mode == kHwMode80211ac) {
    if (GetChannel() >= 34) {
      hostapd_hw_mode = kHostapdHwMode80211a;
    } else {
      hostapd_hw_mode = kHostapdHwMode80211g;
    }
    base::StringAppendF(config_str, "%s=1\n", kHostapdConfigKeyIeee80211ac);

    // TODO(zqiu): Determine VHT Capabilities based on the interface PHY's
    // capababilites.
  } else {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
        "Invalid hardware mode: %s", hw_mode.c_str());
    return false;
  }

  base::StringAppendF(
      config_str, "%s=%s\n", kHostapdConfigKeyHwMode, hostapd_hw_mode.c_str());
  return true;
}

bool Config::AppendHostapdDefaults(ErrorPtr* error,
                                   std::string* config_str) {
  // Driver: NL80211.
  base::StringAppendF(
      config_str, "%s=%s\n", kHostapdConfigKeyDriver, kHostapdDefaultDriver);

  // Fragmentation threshold: disabled.
  base::StringAppendF(config_str,
                      "%s=%d\n",
                      kHostapdConfigKeyFragmThreshold,
                      kHostapdDefaultFragmThreshold);

  // RTS threshold: disabled.
  base::StringAppendF(config_str,
                      "%s=%d\n",
                      kHostapdConfigKeyRtsThreshold,
                      kHostapdDefaultRtsThreshold);

  return true;
}

bool Config::AppendInterface(ErrorPtr* error,
                             std::string* config_str) {
  string interface = GetInterfaceName();
  if (interface.empty()) {
    // Ask manager for unused ap capable device.
    device_ = manager_->GetAvailableDevice();
    if (!device_) {
      chromeos::Error::AddTo(
          error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
          "No device available");
      return false;
    }
  } else {
    device_ = manager_->GetDeviceFromInterfaceName(interface);
    if (!device_) {
      chromeos::Error::AddToPrintf(
          error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
          "Unable to find device for the specified interface [%s]",
          interface.c_str());
      return false;
    }
    if (device_->GetInUsed()) {
      chromeos::Error::AddToPrintf(
          error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
          "Device [%s] for interface [%s] already in use",
          device_->GetDeviceName().c_str(),
          interface.c_str());
      return false;
    }
  }

  // Use the preferred AP interface from the device.
  selected_interface_ = device_->GetPreferredApInterface();
  base::StringAppendF(config_str,
                      "%s=%s\n",
                      kHostapdConfigKeyInterface,
                      selected_interface_.c_str());
  return true;
}

bool Config::AppendSecurityMode(ErrorPtr* error,
                                std::string* config_str) {
  string security_mode = GetSecurityMode();
  if (security_mode == kSecurityModeNone) {
    // Nothing need to be done for open network.
    return true;
  }

  if (security_mode == kSecurityModeRSN) {
    string passphrase = GetPassphrase();
    if (passphrase.empty()) {
      chromeos::Error::AddToPrintf(
          error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
          "Passphrase not set for security mode: %s", security_mode.c_str());
      return false;
    }

    base::StringAppendF(config_str, "%s=2\n", kHostapdConfigKeyWpa);
    base::StringAppendF(config_str,
                        "%s=%s\n",
                        kHostapdConfigKeyRsnPairwise,
                        kHostapdDefaultRsnPairwise);
    base::StringAppendF(config_str,
                        "%s=%s\n",
                        kHostapdConfigKeyWpaKeyMgmt,
                        kHostapdDefaultWpaKeyMgmt);
    base::StringAppendF(config_str,
                        "%s=%s\n",
                        kHostapdConfigKeyWpaPassphrase,
                        passphrase.c_str());
    return true;
  }

  chromeos::Error::AddToPrintf(
      error, FROM_HERE, chromeos::errors::dbus::kDomain, kConfigError,
      "Invalid security mode: %s", security_mode.c_str());
  return false;
}

}  // namespace apmanager
