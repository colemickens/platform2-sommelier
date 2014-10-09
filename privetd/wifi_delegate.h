// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_WIFI_DELEGATE_H_
#define PRIVETD_WIFI_DELEGATE_H_

#include <string>
#include <vector>

namespace privetd {

enum class WifiType {
  kWifi24,
  kWifi50,
};

enum class WifiSetupState {
  kAvalible,
  kCompleted,
  kInProgress,
  kInvalidSsid,
  kInvalidPassword,
};

// Interface to provide wifi functionality for PrivetHandler.
class WifiDelegate {
 public:
  WifiDelegate();
  virtual ~WifiDelegate();

  // Returns current SSID network device connected/trying to connect to.
  // If device in bootstraping mode, returns special setup SSID.
  // May return empty string if wifi is not available.
  virtual std::string GetWifiSsid() const = 0;

  // Returns list of supported wifi types.
  // Currently it's just frequencies.
  virtual std::vector<WifiType> GetWifiTypes() const = 0;

  // Returns true if wifi need to be setup for normal device functioning.
  virtual bool IsWifiRequired() const = 0;

  // Returns the sate of the last setup.
  virtual WifiSetupState GetWifiSetupState() const = 0;

  // Starts wifi setup. Result could be retrieved with |GetWifiSetupState|.
  // Returns false if device is busy and cast start setup.
  virtual bool SetupWifi(const std::string& ssid,
                         const std::string& password) = 0;
};

}  // namespace privetd

#endif  // PRIVETD_WIFI_DELEGATE_H_
