// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_WEBSERV_CLIENT_H_
#define BUFFET_WEBSERV_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>

#include "weave/http_server.h"

namespace dbus {
class Bus;
}

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

namespace buffet {

// Wrapper around libwebserv that implements HttpServer interface.
class WebServClient : public weave::HttpServer {
 public:
  WebServClient(const scoped_refptr<dbus::Bus>& bus,
                chromeos::dbus_utils::AsyncEventSequencer* sequencer);
  ~WebServClient() override;

  // HttpServer implementation.
  void AddOnStateChangedCallback(
      const OnStateChangedCallback& callback) override;
  void AddRequestHandler(const std::string& path_prefix,
                         const OnRequestCallback& callback) override;
  uint16_t GetHttpPort() const override;
  uint16_t GetHttpsPort() const override;
  const chromeos::Blob& GetHttpsCertificateFingerprint() const override;

 private:
  void OnRequest(const OnRequestCallback& callback,
                 std::unique_ptr<libwebserv::Request> request,
                 std::unique_ptr<libwebserv::Response> response);

  void OnResponse(std::unique_ptr<libwebserv::Response> response,
                  int status_code,
                  const std::string& data,
                  const std::string& mime_type);

  void OnProtocolHandlerConnected(
      libwebserv::ProtocolHandler* protocol_handler);

  void OnProtocolHandlerDisconnected(
      libwebserv::ProtocolHandler* protocol_handler);

  uint16_t http_port_{0};
  uint16_t https_port_{0};
  chromeos::Blob certificate_;

  std::vector<OnStateChangedCallback> on_state_changed_callbacks_;

  std::unique_ptr<libwebserv::Server> web_server_;

  base::WeakPtrFactory<WebServClient> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(WebServClient);
};

}  // namespace buffet

#endif  // BUFFET_WEBSERV_CLIENT_H_
