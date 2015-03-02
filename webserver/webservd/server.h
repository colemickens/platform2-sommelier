// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_SERVER_H_
#define WEBSERVER_WEBSERVD_SERVER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/secure_blob.h>

#include "permission_broker/dbus-proxies.h"
#include "webservd/org.chromium.WebServer.Server.h"
#include "webserver/webservd/server_interface.h"

namespace webservd {

class DBusProtocolHandler;
class DBusServerRequest;

// Top-level D-Bus object to interface with the server as a whole.
class Server final : public org::chromium::WebServer::ServerInterface,
                     public ServerInterface {
 public:
  Server(chromeos::dbus_utils::ExportedObjectManager* object_manager,
         const Config& config);
  // Need to off-line the destructor to allow |protocol_handler_map_| to contain
  // a forward-declared pointer to DBusProtocolHandler.
  ~Server();

  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction& cb);

  // Overrides from org::chromium::WebServer::ServerInterface.
  std::string Ping() override;

  // Overrides from webservd::ServerInterface.
  void ProtocolHandlerStarted(ProtocolHandler* handler) override;
  void ProtocolHandlerStopped(ProtocolHandler* handler) override;
  const Config& GetConfig() const override { return config_; }

  scoped_refptr<dbus::Bus> GetBus() { return dbus_object_->GetBus(); }

 private:
  void CreateProtocolHandler(Config::ProtocolHandler* handler_config);
  void InitTlsData();
  void OnPermissionBrokerOnline(org::chromium::PermissionBrokerProxy* proxy);

  org::chromium::WebServer::ServerAdaptor dbus_adaptor_{this};
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;
  std::unique_ptr<org::chromium::PermissionBroker::ObjectManagerProxy>
      permission_broker_object_manager_;

  Config config_;
  int last_protocol_handler_index_{0};
  chromeos::Blob TLS_certificate_;
  chromeos::Blob TLS_certificate_fingerprint_;
  chromeos::SecureBlob TLS_private_key_;

  std::map<ProtocolHandler*,
           std::unique_ptr<DBusProtocolHandler>> protocol_handler_map_;
  // |protocol_handlers_| is currently used to maintain the lifetime of
  // ProtocolHandler object instances. When (if) we start to add/remove
  // protocol handlers dynamically at run-time, it will be used to locate
  // existing handlers so they can be removed.
  std::vector<std::unique_ptr<ProtocolHandler>> protocol_handlers_;

  // File descriptors for the two ends of the pipe used for communicating with
  // remote firewall server (permission_broker), where the remote firewall
  // server will use the read end of the pipe to detect when this process exits.
  int lifeline_read_fd_{-1};
  int lifeline_write_fd_{-1};

  base::WeakPtrFactory<Server> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(Server);
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_SERVER_H_
