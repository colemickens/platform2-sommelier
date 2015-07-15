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

#include "libweave/src/privet/cloud_delegate.h"
#include "libweave/src/privet/security_manager.h"
#include "libweave/src/privet/wifi_bootstrap_manager.h"
#include "weave/device.h"

namespace chromeos {
namespace dbus_utils {
class AsyncEventSequencer;
}
}

namespace libwebserv {
class ProtocolHandler;
class Request;
class Response;
class Server;
}

namespace weave {

class CommandManager;
class DeviceRegistrationInfo;
class StateManager;

namespace privet {

class ApManagerClient;
class CloudDelegate;
class DaemonState;
class DeviceDelegate;
class PeerdClient;
class PrivetHandler;
class SecurityManager;
class ShillClient;

class Manager : public Privet, public CloudDelegate::Observer {
 public:
  Manager();
  ~Manager() override;

  void Start(const weave::Device::Options& options,
             const scoped_refptr<dbus::Bus>& bus,
             ShillClient* shill_client,
             DeviceRegistrationInfo* device,
             CommandManager* command_manager,
             StateManager* state_manager,
             chromeos::dbus_utils::AsyncEventSequencer* sequencer);

  std::string GetCurrentlyConnectedSsid() const;

  void AddOnWifiSetupChangedCallback(
      const OnWifiSetupChangedCallback& callback) override;

  void AddOnPairingChangedCallbacks(
      const OnPairingStartedCallback& on_start,
      const OnPairingEndedCallback& on_end) override;

  void Shutdown();

 private:
  // CloudDelegate::Observer
  void OnDeviceInfoChanged() override;

  void PrivetRequestHandler(std::unique_ptr<libwebserv::Request> request,
                            std::unique_ptr<libwebserv::Response> response);

  void PrivetResponseHandler(std::unique_ptr<libwebserv::Response> response,
                             int status,
                             const base::DictionaryValue& output);

  void HelloWorldHandler(std::unique_ptr<libwebserv::Request> request,
                         std::unique_ptr<libwebserv::Response> response);

  void OnChanged();
  void OnConnectivityChanged(bool online);

  void OnProtocolHandlerConnected(
      libwebserv::ProtocolHandler* protocol_handler);

  void OnProtocolHandlerDisconnected(
      libwebserv::ProtocolHandler* protocol_handler);

  bool disable_security_{false};
  std::unique_ptr<CloudDelegate> cloud_;
  std::unique_ptr<DeviceDelegate> device_;
  std::unique_ptr<SecurityManager> security_;
  std::unique_ptr<ApManagerClient> ap_manager_client_;
  std::unique_ptr<WifiBootstrapManager> wifi_bootstrap_manager_;
  std::unique_ptr<PeerdClient> peerd_client_;
  std::unique_ptr<PrivetHandler> privet_handler_;
  std::unique_ptr<libwebserv::Server> web_server_;

  ScopedObserver<CloudDelegate, CloudDelegate::Observer> cloud_observer_{this};

  base::WeakPtrFactory<Manager> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace privet
}  // namespace weave

#endif  // LIBWEAVE_SRC_PRIVET_PRIVET_MANAGER_H_
