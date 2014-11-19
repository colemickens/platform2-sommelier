// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_WIFI_BOOTSTRAP_MANAGER_H_
#define PRIVETD_WIFI_BOOTSTRAP_MANAGER_H_

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>

#include "privetd/privet_types.h"
#include "privetd/wifi_delegate.h"

namespace privetd {

class WifiBootstrapManager : public WifiDelegate {
 public:
  enum State {
    kDisabled,
    kBootstrapping,
    kMonitoring,
    kConnecting,
  };

  using StateListener = base::Callback<void(State)>;

  explicit WifiBootstrapManager(const std::string& state_file_path);
  ~WifiBootstrapManager() override = default;

  virtual void AddStateChangeListener(const StateListener& cb);

  virtual void Init();

  // Overrides from WifiDelegate.
  bool IsRequired() const override;
  ConnectionState GetConnectionState() const override;
  SetupState GetSetupState() const override;
  bool ConfigureCredentials(const std::string& ssid,
                            const std::string& passphrase) override;
  std::string GetCurrentlyConnectedSsid() const override;
  std::string GetHostedSsid() const override;
  std::vector<WifiType> GetTypes() const override;

 private:
  // These Start* tasks:
  //   1) Do state appropriate work for entering the indicated state.
  //   2) Update the state variable to reflect that we're in a new state
  //   3) Call StateListeners to notify that we've transitioned.
  virtual void StartBootstrapping();
  virtual void StartConnecting(const std::string& ssid,
                               const std::string& passphrase);
  virtual void StartMonitoring();

  virtual void OnApShutdown();
  virtual void OnConnectTimeout();
  virtual void OnConnectSuccess();

  State state_{kDisabled};
  // Setup state is the temporal state of the most recent bootstrapping attempt.
  // It is not persisted to disk.
  SetupState setup_state_{SetupState::kNone};
  std::vector<StateListener> state_listeners_;
  bool have_ever_been_bootstrapped_{false};
  bool currently_online_{false};
  std::string last_configured_ssid_;
  const std::string state_file_path_;
  base::WeakPtrFactory<WifiBootstrapManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WifiBootstrapManager);
};

}  // namespace privetd

#endif  // PRIVETD_WIFI_BOOTSTRAP_MANAGER_H_
