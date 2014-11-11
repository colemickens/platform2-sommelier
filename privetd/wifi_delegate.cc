// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/wifi_delegate.h"

#include <base/logging.h>

namespace privetd {

namespace {

class WifiDelegateImpl : public WifiDelegate {
 public:
  WifiDelegateImpl() {}
  ~WifiDelegateImpl() override {}

  // WifiDelegate methods
  std::string GetWifiSsid() const override { return std::string(); }

  std::vector<WifiType> GetWifiTypes() const override {
    return std::vector<WifiType>();
  }

  bool IsWifiRequired() const override { return false; }

  WifiSetupState GetWifiSetupState() const override {
    return WifiSetupState::kCompleted;
  }

  bool SetupWifi(const std::string& ssid,
                 const std::string& password) override {
    NOTIMPLEMENTED();
    return false;
  }
};

}  // namespace

WifiDelegate::WifiDelegate() {
}

WifiDelegate::~WifiDelegate() {
}

// static
std::unique_ptr<WifiDelegate> WifiDelegate::CreateDefault() {
  return std::unique_ptr<WifiDelegate>(new WifiDelegateImpl);
}

}  // namespace privetd
