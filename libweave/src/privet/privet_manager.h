// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_PRIVET_PRIVET_MANAGER_H_
#define LIBWEAVE_SRC_PRIVET_PRIVET_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <base/scoped_observer.h>
#include <weave/device.h>

#include "libweave/src/privet/cloud_delegate.h"
#include "libweave/src/privet/security_manager.h"
#include "libweave/src/privet/wifi_bootstrap_manager.h"

namespace libwebserv {
class ProtocolHandler;
class Request;
class Response;
class Server;
}

namespace weave {

class CommandManager;
class DeviceRegistrationInfo;
class Mdns;
class Network;
class StateManager;

namespace privet {

class CloudDelegate;
class DaemonState;
class DeviceDelegate;
class PrivetHandler;
class Publisher;
class SecurityManager;

class Manager : public Privet, public CloudDelegate::Observer {
 public:
  Manager();
  ~Manager() override;

  void Start(const weave::Device::Options& options,
             Network* network,
             Mdns* mdns,
             HttpServer* http_server,
             DeviceRegistrationInfo* device,
             CommandManager* command_manager,
             StateManager* state_manager);

  std::string GetCurrentlyConnectedSsid() const;

  void AddOnWifiSetupChangedCallback(
      const OnWifiSetupChangedCallback& callback) override;

  void AddOnPairingChangedCallbacks(
      const OnPairingStartedCallback& on_start,
      const OnPairingEndedCallback& on_end) override;

 private:
  // CloudDelegate::Observer
  void OnDeviceInfoChanged() override;

  void PrivetRequestHandler(const HttpServer::Request& request,
                            const HttpServer::OnReplyCallback& callback);

  void PrivetResponseHandler(const HttpServer::OnReplyCallback& callback,
                             int status,
                             const base::DictionaryValue& output);

  void HelloWorldHandler(const HttpServer::Request& request,
                         const HttpServer::OnReplyCallback& callback);

  void OnChanged();
  void OnConnectivityChanged(bool online);

  void OnHttpServerStatusChanged(const HttpServer& server);

  bool disable_security_{false};
  std::unique_ptr<CloudDelegate> cloud_;
  std::unique_ptr<DeviceDelegate> device_;
  std::unique_ptr<SecurityManager> security_;
  std::unique_ptr<WifiBootstrapManager> wifi_bootstrap_manager_;
  std::unique_ptr<Publisher> publisher_;
  std::unique_ptr<PrivetHandler> privet_handler_;

  ScopedObserver<CloudDelegate, CloudDelegate::Observer> cloud_observer_{this};

  base::WeakPtrFactory<Manager> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace privet
}  // namespace weave

#endif  // LIBWEAVE_SRC_PRIVET_PRIVET_MANAGER_H_
