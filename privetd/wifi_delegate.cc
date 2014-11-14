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
  bool IsRequired() const override { return false; }

  ConnectionState GetState() const override {
    return ConnectionState{ConnectionState::kUnconfigured};
  }

  SetupState GetSetupState() const override {
    return SetupState{SetupState::kNone};
  }

  bool Setup(const std::string& ssid, const std::string& password) override {
    NOTIMPLEMENTED();
    return false;
  }

  std::string GetSsid() const override { return std::string(); }

  std::string GetHostedSsid() const override { return std::string(); }

  std::vector<WifiType> GetTypes() const override { return {}; }
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
