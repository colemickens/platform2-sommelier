// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_PROTOCOL_HANDLER_H_
#define WEBSERVER_WEBSERVD_PROTOCOL_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/strings/string_piece.h>
#include <chromeos/secure_blob.h>

struct MHD_Daemon;

namespace webservd {

class Request;
class RequestHandlerInterface;
class ServerInterface;

// An instance of a handler for particular protocol (http/https) bound to a
// particular port to handle requests on.
class ProtocolHandler final {
 public:
  ProtocolHandler(const std::string& id,
                  ServerInterface* server_interface);
  ~ProtocolHandler();

  // Registers a new request handler for the given URL and request method.
  // Returns a handler ID (GUID).
  std::string AddRequestHandler(
      const std::string& url,
      const std::string& method,
      std::unique_ptr<RequestHandlerInterface> handler);

  // Removes a previously registered handler.
  bool RemoveRequestHandler(const std::string& handler_id);

  // Finds a handler for given URL/Method. This is the method used to look up
  // the handler for incoming HTTP requests.
  // Returns the handler_id or empty string if not found.
  std::string FindRequestHandler(const base::StringPiece& url,
                                 const base::StringPiece& method) const;
  // Binds the socket and listens to HTTP requests on it.
  bool Start(uint16_t port);

  // Binds the socket and listens to HTTPS requests on it.
  bool StartWithTLS(uint16_t port,
                    const chromeos::SecureBlob& private_key,
                    const chromeos::Blob& certificate,
                    const chromeos::Blob& certificate_fingerprint);

  // Stops listening for requests.
  bool Stop();

  // Returns the port this handler listens for requests on.
  uint16_t GetPort() const { return port_; }

  // Returns the protocol name for this handler ("http" or "https").
  const std::string& GetProtocol() const { return protocol_; }

  // Returns the SHA-256 fingerprint of the TLS certificate used for https
  // connection. Returns an empty byte array if this handler is serving http.
  const chromeos::Blob& GetCertificateFingerprint() const {
    return certificate_fingerprint_;
  }

  // Returns the unique request handler ID. This will normally by a GUID, unless
  // this is one of the two default handlers for HTTP/HTTPS protocols with
  // default ports, in which case the IDs will be "http" and "https"
  // respectively.
  const std::string& GetID() const { return id_; }

  // Returns the pointer to the Server object.
  ServerInterface* GetServer() const { return server_interface_; }

  // Methods to store/remove/retrieve pending incoming requests for the duration
  // of the request's processing.
  void AddRequest(Request* request);
  void RemoveRequest(Request* request);
  Request* GetRequest(const std::string& request_id) const;

  // Notification of incoming reply from the request handler.
  void OnResponseDataReceived();

  static const char kHttp[];
  static const char kHttps[];

 private:
  friend class Request;
  friend class ServerHelper;
  class Watcher;
  struct HandlerMapEntry {
    std::string url;
    std::string method;
    std::unique_ptr<RequestHandlerInterface> handler;
  };

  // Called when new data is available on sockets for libmicrohttpd to process.
  void DoWork();
  // Called periodically as requested by libmicrohttpd.
  void TimerCallback();

  // libmicrohttpd daemon class.
  MHD_Daemon* server_{nullptr};
  // A map that stores registered request handlers (the key is handler ID).
  std::map<std::string, HandlerMapEntry> request_handlers_;
  // A map that stores pending requests (the key is request ID).
  std::map<std::string, Request*> requests_;
  // Protocol Handler ID.
  std::string id_;
  // Reference back to the Server.
  ServerInterface* server_interface_{nullptr};
  // The port we are listening to.
  uint16_t port_{0};
  // The protocol name ("http" or "https").
  std::string protocol_;
  // TLS certificate fingerprint (if any).
  chromeos::Blob certificate_fingerprint_;
  // File descriptor watchers for current active sockets.
  std::vector<std::unique_ptr<Watcher>> watchers_;
  // Set to true when a timer request is scheduled.
  bool timer_scheduled_{false};

  base::WeakPtrFactory<ProtocolHandler> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ProtocolHandler);
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_PROTOCOL_HANDLER_H_
