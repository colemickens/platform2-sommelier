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
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/secure_blob.h>

#include "webservd/org.chromium.WebServer.Server.h"
#include "webserver/webservd/server_interface.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // dbus_utils
}  // chromeos

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
  void CreateProtocolHandler(const std::string& id,
                             const Config::ProtocolHandler& handler_config);
  void InitTlsData();

  org::chromium::WebServer::ServerAdaptor dbus_adaptor_{this};
  std::unique_ptr<chromeos::dbus_utils::DBusObject> dbus_object_;
  Config config_;
  int last_protocol_handler_index_{0};
  chromeos::Blob TLS_certificate_;
  chromeos::Blob TLS_certificate_fingerprint_;
  chromeos::SecureBlob TLS_private_key_;

  std::map<ProtocolHandler*,
           std::unique_ptr<DBusProtocolHandler>> protocol_handler_map_;
  std::map<std::string, std::unique_ptr<ProtocolHandler>> protocol_handlers_;

  DISALLOW_COPY_AND_ASSIGN(Server);
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_SERVER_H_
