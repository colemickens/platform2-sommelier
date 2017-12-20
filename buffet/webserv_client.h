// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_WEBSERV_CLIENT_H_
#define BUFFET_WEBSERV_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <weave/provider/http_server.h>

namespace dbus {
class Bus;
}

namespace brillo {
namespace dbus_utils {
class AsyncEventSequencer;
}
}  // namespace brillo

namespace libwebserv {
class ProtocolHandler;
class Request;
class Response;
class Server;
}  // namespace libwebserv

namespace buffet {

// Wrapper around libwebserv that implements HttpServer interface.
class WebServClient : public weave::provider::HttpServer {
 public:
  WebServClient(const scoped_refptr<dbus::Bus>& bus,
                brillo::dbus_utils::AsyncEventSequencer* sequencer,
                const base::Closure& server_available_callback);
  ~WebServClient() override;

  // HttpServer implementation.
  void AddHttpRequestHandler(const std::string& path,
                             const RequestHandlerCallback& callback) override;
  void AddHttpsRequestHandler(const std::string& path,
                              const RequestHandlerCallback& callback) override;

  uint16_t GetHttpPort() const override;
  uint16_t GetHttpsPort() const override;
  base::TimeDelta GetRequestTimeout() const override;
  std::vector<uint8_t> GetHttpsCertificateFingerprint() const override;

 private:
  void OnRequest(const RequestHandlerCallback& callback,
                 std::unique_ptr<libwebserv::Request> request,
                 std::unique_ptr<libwebserv::Response> response);

  void OnProtocolHandlerConnected(
      libwebserv::ProtocolHandler* protocol_handler);

  void OnProtocolHandlerDisconnected(
      libwebserv::ProtocolHandler* protocol_handler);

  uint16_t http_port_{0};
  uint16_t https_port_{0};
  std::vector<uint8_t> certificate_;

  std::unique_ptr<libwebserv::Server> web_server_;
  base::Closure server_available_callback_;

  base::WeakPtrFactory<WebServClient> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(WebServClient);
};

}  // namespace buffet

#endif  // BUFFET_WEBSERV_CLIENT_H_
