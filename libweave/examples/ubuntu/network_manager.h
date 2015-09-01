// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_EXAMPLES_UBUNTU_NETWORK_MANAGER_H_
#define LIBWEAVE_EXAMPLES_UBUNTU_NETWORK_MANAGER_H_

#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <base/time/time.h>
#include <weave/network.h>

namespace weave {

class TaskRunner;

namespace examples {

// Basic weave::Network implementation.
// Production version of SSL socket needs secure server certificate check.
class NetworkImpl : public Network {
 public:
  explicit NetworkImpl(TaskRunner* task_runner);
  ~NetworkImpl();

  void AddOnConnectionChangedCallback(
      const OnConnectionChangedCallback& listener) override;
  bool ConnectToService(const std::string& ssid,
                        const std::string& passphrase,
                        const base::Closure& on_success,
                        ErrorPtr* error) override;
  NetworkState GetConnectionState() const override;
  void EnableAccessPoint(const std::string& ssid) override;
  void DisableAccessPoint() override;
  std::unique_ptr<Stream> OpenSocketBlocking(const std::string& host,
                                             uint16_t port) override;
  void CreateTlsStream(
      std::unique_ptr<Stream> stream,
      const std::string& host,
      const base::Callback<void(std::unique_ptr<Stream>)>& success_callback,
      const base::Callback<void(const Error*)>& error_callback) override;

 private:
  void TryToConnect(const std::string& ssid,
                    const std::string& passphrase,
                    int pid,
                    base::Time until,
                    const base::Closure& on_success);
  void NotifyNetworkChanged();

  bool hostapd_started_{false};
  TaskRunner* task_runner_{nullptr};
  std::vector<OnConnectionChangedCallback> callbacks_;

  base::WeakPtrFactory<NetworkImpl> weak_ptr_factory_{this};
};

}  // namespace examples
}  // namespace weave

#endif  // LIBWEAVE_EXAMPLES_UBUNTU_NETWORK_MANAGER_H_
