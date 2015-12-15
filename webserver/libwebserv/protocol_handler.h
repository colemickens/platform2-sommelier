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

#ifndef WEBSERVER_LIBWEBSERV_PROTOCOL_HANDLER_H_
#define WEBSERVER_LIBWEBSERV_PROTOCOL_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <brillo/errors/error.h>
#include <brillo/secure_blob.h>
#include <brillo/streams/stream.h>
#include <dbus/object_path.h>

#include <libwebserv/export.h>
#include <libwebserv/request_handler_interface.h>

namespace org {
namespace chromium {
namespace WebServer {

class ProtocolHandlerProxyInterface;

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace libwebserv {

class DBusServer;
class Request;

// Wrapper around a protocol handler (e.g. HTTP or HTTPs).
// ProtocolHandler allows consumers to add request handlers on a given protocol.
// When the ProtocolHandler is connected, allows users to read port and protocol
// information.
class LIBWEBSERV_EXPORT ProtocolHandler final {
 public:
  ProtocolHandler(const std::string& name, DBusServer* server);
  ~ProtocolHandler();

  // Returns true if the protocol handler object is connected to the web server
  // daemon's proxy object and is capable of processing incoming requests.
  bool IsConnected() const { return !proxies_.empty(); }

  // Handler's name identifier (as provided in "name" setting of config file).
  // Standard/default handler names are "http" and "https".
  std::string GetName() const;

  // Returns the ports the handler is bound to. There could be multiple.
  // If the handler is not connected to the server, this will return an empty
  // set.
  std::set<uint16_t> GetPorts() const;

  // Returns the transport protocol that is served by this handler.
  // Can be either "http" or "https".
  // If the handler is not connected to the server, this will return an empty
  // set.
  std::set<std::string> GetProtocols() const;

  // Returns a SHA-256 fingerprint of HTTPS certificate used. Returns an empty
  // byte buffer if this handler does not serve the HTTPS protocol.
  // If the handler is not connected to the server, this will return an empty
  // array.
  brillo::Blob GetCertificateFingerprint() const;

  // Adds a request handler for given |url|. If the |url| ends with a '/', this
  // makes the handler respond to any URL beneath this path.
  // Note that it is not possible to add a specific handler just for the root
  // path "/". Doing so means "respond to any URL".
  // |method| is optional request method verb, such as "GET" or "POST".
  // If |method| is empty, the handler responds to any request verb.
  // If there are more than one handler for a given request, the most specific
  // match is chosen. For example, if there are the following handlers provided:
  //    - A["/foo/",  ""]
  //    - B["/foo/bar", "GET"]
  //    - C["/foo/bar", ""]
  // Here is what handlers are called when making certain requests:
  //    - GET("/foo/bar")   => B[]
  //    - POST("/foo/bar")  => C[]
  //    - PUT("/foo/bar")   => C[]
  //    - GET("/foo/baz")   => A[]
  //    - GET("/foo")       => 404 Not Found
  // This functions returns a handler ID which can be used later to remove
  // the handler.
  //
  // The handler registration information is stored inside ProtocolHandler and
  // is used to register the handlers with the web server daemon when it becomes
  // available. This also happens when the web server goes away and then comes
  // back (e.g. restarted). So, there is no need to re-register the handlers
  // once the web server process is restarted.
  int AddHandler(const std::string& url,
                 const std::string& method,
                 std::unique_ptr<RequestHandlerInterface> handler);

  // Similar to AddHandler() above but the handler is just a callback function.
  int AddHandlerCallback(
      const std::string& url,
      const std::string& method,
      const base::Callback<RequestHandlerInterface::HandlerSignature>&
          handler_callback);

  // Removes the handler with the specified |handler_id|.
  // Returns false if the handler with the given ID is not found.
  bool RemoveHandler(int handler_id);

  static const char kHttp[];
  static const char kHttps[];

 private:
  friend class FileInfo;
  friend class DBusServer;
  friend class ResponseImpl;

  using ProtocolHandlerProxyInterface =
      org::chromium::WebServer::ProtocolHandlerProxyInterface;

  struct LIBWEBSERV_PRIVATE HandlerMapEntry {
    std::string url;
    std::string method;
    std::map<ProtocolHandlerProxyInterface*, std::string> remote_handler_ids;
    std::unique_ptr<RequestHandlerInterface> handler;
  };

  // Called by the DBusServer class when the D-Bus proxy object gets connected
  // to the web server daemon.
  LIBWEBSERV_PRIVATE void Connect(ProtocolHandlerProxyInterface* proxy);

  // Called by the DBusServer class when the D-Bus proxy object gets
  // disconnected from the web server daemon.
  LIBWEBSERV_PRIVATE void Disconnect(const dbus::ObjectPath& object_path);

  // Asynchronous callbacks to handle successful or failed request handler
  // registration over D-Bus.
  LIBWEBSERV_PRIVATE void AddHandlerSuccess(
      int handler_id,
      ProtocolHandlerProxyInterface* proxy,
      const std::string& remote_handler_id);
  LIBWEBSERV_PRIVATE void AddHandlerError(int handler_id,
                                          brillo::Error* error);

  // Called by DBusServer when an incoming request is dispatched.
  LIBWEBSERV_PRIVATE bool ProcessRequest(const std::string& protocol_handler_id,
                                         const std::string& remote_handler_id,
                                         const std::string& request_id,
                                         std::unique_ptr<Request> request,
                                         brillo::ErrorPtr* error);

  // Called by Response object to finish the request and send response data.
  LIBWEBSERV_PRIVATE void CompleteRequest(
      const std::string& request_id,
      int status_code,
      const std::multimap<std::string, std::string>& headers,
      brillo::StreamPtr data_stream);

  // Makes a call to the (remote) web server request handler over D-Bus to
  // obtain the file content of uploaded file (identified by |file_id|) during
  // request with |request_id|.
  LIBWEBSERV_PRIVATE void GetFileData(
      const std::string& request_id,
      int file_id,
      const base::Callback<void(brillo::StreamPtr)>& success_callback,
      const base::Callback<void(brillo::Error*)>& error_callback);

  // A helper method to obtain a corresponding protocol handler D-Bus proxy for
  // outstanding request with ID |request_id|.
  LIBWEBSERV_PRIVATE ProtocolHandlerProxyInterface*
      GetRequestProtocolHandlerProxy(const std::string& request_id) const;

  // Protocol Handler name.
  std::string name_;
  // Back reference to the server object.
  DBusServer* server_{nullptr};
  // Handler data map. The key is the client-facing request handler ID returned
  // by AddHandler() when registering the handler.
  std::map<int, HandlerMapEntry> request_handlers_;
  // The counter to generate new handler IDs.
  int last_handler_id_{0};
  // Map of remote handler IDs (GUID strings) to client-facing request handler
  // IDs (int) which are returned by AddHandler() and used as a key in
  // |request_handlers_|.
  std::map<std::string, int> remote_handler_id_map_;
  // Remote D-Bus proxies for the server protocol handler objects.
  // There could be multiple protocol handlers with the same name (to make
  // it possible to server the same requests on different ports, for example).
  std::map<dbus::ObjectPath, ProtocolHandlerProxyInterface*> proxies_;
  // A map of request ID to protocol handler ID. Used to locate the appropriate
  // protocol handler D-Bus proxy for given request.
  std::map<std::string, std::string> request_id_map_;

  base::WeakPtrFactory<ProtocolHandler> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ProtocolHandler);
};

}  // namespace libwebserv

#endif  // WEBSERVER_LIBWEBSERV_PROTOCOL_HANDLER_H_
