// Copyright 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef WEBSERVER_LIBWEBSERV_SERVER_H_
#define WEBSERVER_LIBWEBSERV_SERVER_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <dbus/bus.h>
#include <brillo/dbus/async_event_sequencer.h>
#include <brillo/dbus/dbus_object.h>
#include <libwebserv/export.h>
#include <libwebserv/request_handler_interface.h>

namespace org {
namespace chromium {
namespace WebServer {

class ObjectManagerProxy;
class ProtocolHandlerProxyInterface;
class RequestHandlerAdaptor;
class ServerProxyInterface;

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace libwebserv {

// Top-level wrapper class around HTTP server and provides an interface to
// the web server.
class LIBWEBSERV_EXPORT Server final {
 public:
  Server();
  ~Server();

  // Establish a connection to the system webserver.
  // |service_name| is the well known D-Bus name of the client's process, used
  // to expose a callback D-Bus object the web server calls back with incoming
  // requests.
  // |on_server_online| and |on_server_offline| will notify the caller when the
  // server comes up and down.
  // Note that we can Connect() even before the webserver attaches to D-Bus,
  // and appropriate state will be built up when the webserver appears on D-Bus.
  void Connect(
      const scoped_refptr<dbus::Bus>& bus,
      const std::string& service_name,
      const brillo::dbus_utils::AsyncEventSequencer::CompletionAction& cb,
      const base::Closure& on_server_online,
      const base::Closure& on_server_offline);

  // Disconnects from the web server and removes the library interface from
  // D-Bus.
  void Disconnect();

  // A helper method that returns the default handler for "http".
  ProtocolHandler* GetDefaultHttpHandler();

  // A helper method that returns the default handler for "https".
  ProtocolHandler* GetDefaultHttpsHandler();

  // Returns an existing protocol handler by name.  If the handler with the
  // requested |name| does not exist, a new one will be created.
  //
  // The created handler is purely client side, and depends on the server
  // being configured to open a corresponding handler with the given name.
  // Because clients and the server come up asynchronously, we allow clients
  // to register anticipated handlers before server starts up.
  ProtocolHandler* GetProtocolHandler(const std::string& name);

  // Returns true if the web server daemon is connected to DBus and our
  // connection to it has been established.
  bool IsConnected() const { return proxy_ != nullptr; }

  // Set a user-callback to be invoked when a protocol handler is connect to the
  // server daemon.  Multiple calls to this method will overwrite previously set
  // callbacks.
  void OnProtocolHandlerConnected(
      const base::Callback<void(ProtocolHandler*)>& callback);

  // Set a user-callback to be invoked when a protocol handler is disconnected
  // from the server daemon (e.g. on shutdown).  Multiple calls to this method
  // will overwrite previously set callbacks.
  void OnProtocolHandlerDisconnected(
      const base::Callback<void(ProtocolHandler*)>& callback);

  // Returns the default request timeout used to process incoming requests.
  // The reply to an incoming request should be sent within this timeout or
  // else the web server will automatically abort the connection. If the timeout
  // is not set, the returned value will be base::TimeDelta::Max().
  base::TimeDelta GetDefaultRequestTimeout() const;

 private:
  friend class ProtocolHandler;
  class RequestHandler;

  // Handler invoked when a connection is established to web server daemon.
  LIBWEBSERV_PRIVATE void Online(
      org::chromium::WebServer::ServerProxyInterface* server);

  // Handler invoked when the web server daemon connection is dropped.
  LIBWEBSERV_PRIVATE void Offline(const dbus::ObjectPath& object_path);

  // Handler invoked when a new protocol handler D-Bus proxy object becomes
  // available.
  LIBWEBSERV_PRIVATE void ProtocolHandlerAdded(
      org::chromium::WebServer::ProtocolHandlerProxyInterface* handler);

  // Handler invoked when a protocol handler D-Bus proxy object disappears.
  LIBWEBSERV_PRIVATE void ProtocolHandlerRemoved(
      const dbus::ObjectPath& object_path);

  // Looks up a protocol handler by ID. If not found, returns nullptr.
  LIBWEBSERV_PRIVATE ProtocolHandler* GetProtocolHandlerByID(
      const std::string& id) const;

  // Private implementation of D-Bus RequestHandlerInterface called by the web
  // server daemon whenever a new request is available to be processed.
  std::unique_ptr<RequestHandler> request_handler_;
  // D-Bus object adaptor for RequestHandlerInterface.
  std::unique_ptr<org::chromium::WebServer::RequestHandlerAdaptor>
      dbus_adaptor_;
  // D-Bus object to handler registration of RequestHandlerInterface.
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;

  // A mapping of protocol handler name to the associated object.
  std::map<std::string, std::unique_ptr<ProtocolHandler>>
      protocol_handlers_names_;
  // A mapping of protocol handler IDs to the associated object.
  std::map<std::string, ProtocolHandler*> protocol_handlers_ids_;
  // A map between D-Bus object path of protocol handler and remote protocol
  // handler ID.
  std::map<dbus::ObjectPath, std::string> protocol_handler_id_map_;

  // User-specified callbacks for server and protocol handler life-time events.
  base::Closure on_server_online_;
  base::Closure on_server_offline_;
  base::Callback<void(ProtocolHandler*)> on_protocol_handler_connected_;
  base::Callback<void(ProtocolHandler*)> on_protocol_handler_disconnected_;

  // D-Bus object manager proxy that receives notification of web server
  // daemon's D-Bus object creation and destruction.
  std::unique_ptr<org::chromium::WebServer::ObjectManagerProxy> object_manager_;

  // D-Bus proxy for the web server main object.
  org::chromium::WebServer::ServerProxyInterface* proxy_{nullptr};

  // D-Bus service name used by the daemon hosting this object.
  std::string service_name_;

  DISALLOW_COPY_AND_ASSIGN(Server);
};

}  // namespace libwebserv

#endif  // WEBSERVER_LIBWEBSERV_SERVER_H_
