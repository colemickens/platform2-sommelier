// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_WIFI_BOOTSTRAP_MANAGER_H_
#define PRIVETD_WIFI_BOOTSTRAP_MANAGER_H_

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "privetd/daemon_state.h"
#include "privetd/privet_types.h"
#include "privetd/wifi_delegate.h"
#include "privetd/wifi_ssid_generator.h"

namespace privetd {

class ApManagerClient;
class CloudDelegate;
class DeviceDelegate;
class ShillClient;

class WifiBootstrapManager : public WifiDelegate {
 public:
  enum State {
    kDisabled,
    kBootstrapping,
    kMonitoring,
    kConnecting,
  };

  using StateListener = base::Callback<void(State)>;

  WifiBootstrapManager(DaemonState* state_store,
                       ShillClient* shill_client,
                       ApManagerClient* ap_manager_client,
                       const DeviceDelegate* device,
                       const CloudDelegate* gcd,
                       uint32_t connect_timeout_seconds,
                       uint32_t bootstrap_timeout_seconds,
                       uint32_t monitor_timeout_seconds);
  ~WifiBootstrapManager() override = default;
  virtual void Init();
  void RegisterStateListener(const StateListener& listener);

  // Overrides from WifiDelegate.
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
  // These End* tasks perform cleanup on leaving indicated state.
  void StartBootstrapping();
  void EndBootstrapping();

  void StartConnecting(const std::string& ssid, const std::string& passphrase);
  void EndConnecting();

  void StartMonitoring();
  void EndMonitoring();

  // Update the current state, post tasks to notify listeners accordingly to
  // the MessageLoop.
  void UpdateState(State new_state);
  void NotifyStateListeners(State new_state) const;

  // If we've been bootstrapped successfully before, and we're bootstrapping
  // again because we slipped offline for a sufficiently longtime, we want
  // to return to monitoring mode periodically in case our connectivity issues
  // were temporary.
  void OnBootstrapTimeout();
  void OnConnectTimeout();
  void OnConnectSuccess(const std::string& ssid);
  void OnConnectivityChange(bool is_connected);
  void OnMonitorTimeout();

  State state_{kDisabled};
  // Setup state is the temporal state of the most recent bootstrapping attempt.
  // It is not persisted to disk.
  SetupState setup_state_{SetupState::kNone};
  DaemonState* state_store_;
  ShillClient* shill_client_;
  ApManagerClient* ap_manager_client_;
  WifiSsidGenerator ssid_generator_;

  uint32_t connect_timeout_seconds_;
  uint32_t bootstrap_timeout_seconds_;
  uint32_t monitor_timeout_seconds_;
  std::vector<StateListener> state_listeners_;
  bool have_ever_been_bootstrapped_{false};
  bool currently_online_{false};
  std::string last_configured_ssid_;

  // Helps to reset irrelevant tasks switching state.
  base::WeakPtrFactory<WifiBootstrapManager> tasks_weak_factory_{this};

  base::WeakPtrFactory<WifiBootstrapManager> lifetime_weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WifiBootstrapManager);
};

}  // namespace privetd

#endif  // PRIVETD_WIFI_BOOTSTRAP_MANAGER_H_
