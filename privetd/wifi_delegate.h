// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_WIFI_DELEGATE_H_
#define PRIVETD_WIFI_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "privetd/privet_types.h"

namespace privetd {

enum class WifiType {
  kWifi24,
  kWifi50,
};

// Interface to provide WiFi functionality for PrivetHandler.
class WifiDelegate {
 public:
  WifiDelegate() = default;
  virtual ~WifiDelegate() = default;

  // Returns status of the WiFi connection.
  virtual ConnectionState GetConnectionState() const = 0;

  // Returns status of the last WiFi setup.
  virtual SetupState GetSetupState() const = 0;

  // Starts WiFi setup. Device should try to connect to provided SSID and
  // password and store them on success. Result of setup should be available
  // using GetSetupState().
  // Returns false only if device is busy. Any other failures should be stored
  // in GetSetupState().
  virtual bool ConfigureCredentials(const std::string& ssid,
                                    const std::string& password) = 0;

  // Returns SSID of the currently configured WiFi network. Empty string, if
  // WiFi has not been configured yet.
  virtual std::string GetCurrentlyConnectedSsid() const = 0;

  // Returns SSID of the WiFi network hosted by this device. Empty if device is
  // not in setup or P2P modes.
  virtual std::string GetHostedSsid() const = 0;

  // Returns list of supported WiFi types. Currently it's just frequencies.
  virtual std::vector<WifiType> GetTypes() const = 0;
};

}  // namespace privetd

#endif  // PRIVETD_WIFI_DELEGATE_H_
