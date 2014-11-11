// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/config.h"

#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

using chromeos::ErrorPtr;
using std::string;

namespace apmanager {

// static
const char Config::kHostapdConfigKeyBridgeInterface[] = "bridge";
const char Config::kHostapdConfigKeyChannel[] = "channel";
const char Config::kHostapdConfigKeyControlInterface[] = "ctrl_interface";
const char Config::kHostapdConfigKeyDriver[] = "driver";
const char Config::kHostapdConfigKeyFragmThreshold[] = "fragm_threshold";
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

Config::Config(
    const string& service_path,
    chromeos::dbus_utils::ExportedObjectManager* object_manager,
    chromeos::dbus_utils::AsyncEventSequencer* sequencer)
    : org::chromium::apmanager::ConfigAdaptor(this),
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

bool Config::GenerateConfigFile(ErrorPtr* error, string* config_str) {
  // SSID.
  string ssid = GetSsid();
  if (ssid.empty()) {
    SetError(__func__, "SSID not specified", error);
    return false;
  }
  base::StringAppendF(
      config_str, "%s=%s\n", kHostapdConfigKeySsid, ssid.c_str());

  // Channel.
  base::StringAppendF(
      config_str, "%s=%d\n", kHostapdConfigKeyChannel, GetChannel());

  // Hardware mode.
  if (!AppendHwMode(error, config_str)) {
    return false;
  }

  // Interface.
  if (!AppendInterface(error, config_str)) {
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

bool Config::AppendHwMode(chromeos::ErrorPtr* error, std::string* config_str) {
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

    // TODO(zqiu): Determine HT Capabilities based on the interface PHY's
    // capababilites.
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
    SetError(__func__,
             base::StringPrintf("Invalid hardware mode: %s", hw_mode.c_str()),
             error);
    return false;
  }

  base::StringAppendF(
      config_str, "%s=%s\n", kHostapdConfigKeyHwMode, hostapd_hw_mode.c_str());
  return true;
}

bool Config::AppendHostapdDefaults(chromeos::ErrorPtr* error,
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

bool Config::AppendInterface(chromeos::ErrorPtr* error,
                             std::string* config_str) {
  string interface = GetInterfaceName();
  if (interface.empty()) {
    // TODO(zqiu): Ask manager for available ap mode interface.
    return false;
  }

  base::StringAppendF(
      config_str, "%s=%s\n", kHostapdConfigKeyInterface, interface.c_str());
  return true;
}

bool Config::AppendSecurityMode(chromeos::ErrorPtr* error,
                                std::string* config_str) {
  string security_mode = GetSecurityMode();
  if (security_mode == kSecurityModeNone) {
    // Nothing need to be done for open network.
    return true;
  }

  if (security_mode == kSecurityModeRSN) {
    string passphrase = GetPassphrase();
    if (passphrase.empty()) {
      SetError(__func__,
               base::StringPrintf("Passphrase not set for security mode: %s",
                                  security_mode.c_str()),
               error);
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

  SetError(__func__,
           base::StringPrintf("Invalid security mode: %s",
                              security_mode.c_str()),
           error);
  return false;
}

// static.
void Config::SetError(const string& method,
                      const string& message,
                      chromeos::ErrorPtr* error) {
  chromeos::Error::AddToPrintf(
      error, chromeos::errors::dbus::kDomain, kConfigError,
      "%s : %s", method.c_str(), message.c_str());
}

}  // namespace apmanager
