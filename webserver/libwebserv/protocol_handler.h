// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_LIBWEBSERV_PROTOCOL_HANDLER_H_
#define WEBSERVER_LIBWEBSERV_PROTOCOL_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/errors/error.h>
#include <chromeos/secure_blob.h>

#include <libwebserv/export.h>
#include <libwebserv/request_handler_interface.h>

namespace org {
namespace chromium {
namespace WebServer {

class ProtocolHandlerProxy;

}  // namespace WebServer
}  // namespace chromium
}  // namespace org

namespace libwebserv {

class Server;
class Request;

// Wrapper around a protocol handler (e.g. HTTP or HTTPs).
// ProtocolHandler allows consumers to add request handlers on a given protocol.
// When the ProtocolHandler is connected, allows users to read port and protocol
// information.
class LIBWEBSERV_EXPORT ProtocolHandler final {
 public:
  explicit ProtocolHandler(const std::string& id, Server* server);
  ~ProtocolHandler();

  // Returns true if the protocol handler object is connected to the web server
  // daemon's proxy object and is capable of processing incoming requests.
  bool IsConnected() const { return proxy_ != nullptr; }

  // Handler's unique ID. This is generally a GUID string, except for the
  // default HTTP and HTTPS handlers which have IDs of "http" and "https"
  // respectively.
  std::string GetID() const;

  // Returns the port the handler is bound to.
  // ATTENTION: The handler must be connected to the web server before this
  // method can be called.
  uint16_t GetPort() const;

  // Returns the transport protocol that is served by this handler.
  // Can be either "http" or "https".
  // ATTENTION: The handler must be connected to the web server before this
  // method can be called.
  std::string GetProtocol() const;

  // Returns a SHA-256 fingerprint of HTTPS certificate used. Returns an empty
  // byte buffer if this handler does not serve the HTTPS protocol.
  // ATTENTION: The handler must be connected to the web server before this
  // method can be called.
  chromeos::Blob GetCertificateFingerprint() const;

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
  friend class Server;
  friend class Response;

  struct LIBWEBSERV_PRIVATE HandlerMapEntry {
    std::string url;
    std::string method;
    std::string remote_handler_id;
    std::unique_ptr<RequestHandlerInterface> handler;
  };

  // Called by the Server class when the D-Bus proxy object gets connected
  // to the web server daemon.

  LIBWEBSERV_PRIVATE void Connect(
      org::chromium::WebServer::ProtocolHandlerProxy* proxy);
  // Called by the Server class when the D-Bus proxy object gets disconnected
  // from the web server daemon.
  LIBWEBSERV_PRIVATE void Disconnect();

  // Asynchronous callbacks to handle successful or failed request handler
  // registration over D-Bus.
  LIBWEBSERV_PRIVATE void AddHandlerSuccess(
      int handler_id, const std::string& remote_handler_id);
  LIBWEBSERV_PRIVATE void AddHandlerError(int handler_id,
                                          chromeos::Error* error);

  // Called by Server when an incoming request is dispatched.
  LIBWEBSERV_PRIVATE bool ProcessRequest(const std::string& remote_handler_id,
                                         const std::string& request_id,
                                         std::unique_ptr<Request> request,
                                         chromeos::ErrorPtr* error);

  // Called by Response object to finish the request and send response data.
  LIBWEBSERV_PRIVATE void CompleteRequest(
      const std::string& request_id,
      int status_code,
      const std::multimap<std::string, std::string>& headers,
      const std::vector<uint8_t>& data);

  // Makes a call to the (remote) web server request handler over D-Bus to
  // obtain the file content of uploaded file (identified by |file_id|) during
  // request with |request_id|.
  LIBWEBSERV_PRIVATE void GetFileData(
    const std::string& request_id,
    int file_id,
    const base::Callback<void(const std::vector<uint8_t>&)>& success_callback,
    const base::Callback<void(chromeos::Error*)>& error_callback);

  // Protocol Handler unique ID.
  std::string id_;
  // Back reference to the server object.
  Server* server_{nullptr};
  // Handler data map. The key is the client-facing request handler ID returned
  // by AddHandler() when registering the handler.
  std::map<int, HandlerMapEntry> request_handlers_;
  // Map of remote handler IDs (GUID strings) to client-facing request handler
  // IDs (int) which are returned by AddHandler() and used as a key in
  // |request_handlers_|.
  std::map<std::string, int> remote_handler_id_map_;
  // The counter to generate new handler IDs.
  int last_handler_id_{0};
  // Remove D-Bus proxy for the server protocol handler object.
  org::chromium::WebServer::ProtocolHandlerProxy* proxy_{nullptr};

  base::WeakPtrFactory<ProtocolHandler> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ProtocolHandler);
};

}  // namespace libwebserv

#endif  // WEBSERVER_LIBWEBSERV_PROTOCOL_HANDLER_H_
