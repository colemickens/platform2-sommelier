// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/wifi_delegate.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>

namespace privetd {

namespace {

class WifiDelegateImpl : public WifiDelegate {
 public:
  WifiDelegateImpl() {}
  ~WifiDelegateImpl() override {}

  // WifiDelegate methods
  bool IsRequired() const override { return false; }

  ConnectionState GetConnectionState() const override { return state_; }

  SetupState GetSetupState() const override { return setup_state_; }

  bool ConfigureCredentials(const std::string& ssid,
                            const std::string& password) override {
    if (setup_state_.status == SetupState::kInProgress)
      return false;
    VLOG(1) << "WiFi Setup started. ssid: " << ssid << ", pass:" << password;
    setup_state_ = SetupState(SetupState::kInProgress);
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::Bind(&WifiDelegateImpl::OnSetupDone,
                              weak_factory_.GetWeakPtr(), ssid),
        base::TimeDelta::FromSeconds(5));
    return true;
  }

  std::string GetCurrentlyConnectedSsid() const override { return ssid_; }

  std::string GetHostedSsid() const override { return std::string(); }

  std::vector<WifiType> GetTypes() const override {
    return {WifiType::kWifi24};
  }

 private:
  void OnSetupDone(const std::string& ssid) {
    VLOG(1) << "WiFi Setup done";
    setup_state_ = SetupState(SetupState::kSuccess);
    state_ = ConnectionState(ConnectionState::kOnline);
    ssid_ = ssid;
  }

  // Primary state of WiFi.
  ConnectionState state_{ConnectionState::kUnconfigured};

  // State of the current or last setup.
  SetupState setup_state_{SetupState::kNone};

  // The last successfully used WiFi ssid.
  std::string ssid_;

  base::WeakPtrFactory<WifiDelegateImpl> weak_factory_{this};
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
