// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_PRIVET_PRIVET_MANAGER_H_
#define BUFFET_PRIVET_PRIVET_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <base/scoped_observer.h>
#include <chromeos/daemons/dbus_daemon.h>

#include "buffet/privet/cloud_delegate.h"

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

namespace privetd {

class PrivetdConfigParser;
class DaemonState;
class CloudDelegate;
class DeviceDelegate;
class SecurityManager;
class ShillClient;
class ApManagerClient;
class WifiBootstrapManager;
class PeerdClient;
class PrivetHandler;

class Manager : public chromeos::DBusServiceDaemon,
                public CloudDelegate::Observer {
 public:
  Manager(bool disable_security,
          bool enable_ping,
          const std::set<std::string>& device_whitelist,
          const base::FilePath& config_path,
          const base::FilePath& state_path);
  ~Manager() override;

  void RegisterDBusObjectsAsync(
      chromeos::dbus_utils::AsyncEventSequencer* sequencer) override;

  void OnShutdown(int* return_code) override;

  void OnDeviceInfoChanged() override;

 private:
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

  bool disable_security_;
  bool enable_ping_;
  std::unique_ptr<PrivetdConfigParser> parser_;
  std::set<std::string> device_whitelist_;
  base::FilePath config_path_;
  std::unique_ptr<DaemonState> state_store_;
  std::unique_ptr<CloudDelegate> cloud_;
  std::unique_ptr<DeviceDelegate> device_;
  std::unique_ptr<SecurityManager> security_;
  std::unique_ptr<ShillClient> shill_client_;
  std::unique_ptr<ApManagerClient> ap_manager_client_;
  std::unique_ptr<WifiBootstrapManager> wifi_bootstrap_manager_;
  std::unique_ptr<PeerdClient> peerd_client_;
  std::unique_ptr<PrivetHandler> privet_handler_;
  std::unique_ptr<libwebserv::Server> web_server_;

  ScopedObserver<CloudDelegate, CloudDelegate::Observer> cloud_observer_{this};

  base::WeakPtrFactory<Manager> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace privetd

#endif  // BUFFET_PRIVET_PRIVET_MANAGER_H_
