// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/webserv_client.h"

#include <memory>
#include <string>

#include <libwebserv/protocol_handler.h>
#include <libwebserv/request.h>
#include <libwebserv/response.h>
#include <libwebserv/server.h>

#include "buffet/dbus_constants.h"

namespace buffet {

namespace {

class RequestImpl : public weave::HttpServer::Request {
 public:
  explicit RequestImpl(std::unique_ptr<libwebserv::Request> request)
      : request_{std::move(request)} {}
  ~RequestImpl() override {}

  // HttpServer::Request implementation.
  const std::string& GetPath() const override { return request_->GetPath(); }
  std::string GetFirstHeader(const std::string& name) const override {
    return request_->GetFirstHeader(name);
  }
  const std::vector<uint8_t>& GetData() const override {
    return request_->GetData();
  }

 private:
  std::unique_ptr<libwebserv::Request> request_;

  DISALLOW_COPY_AND_ASSIGN(RequestImpl);
};

}  // namespace

WebServClient::~WebServClient() {
  web_server_->Disconnect();
}

void WebServClient::AddOnStateChangedCallback(
    const OnStateChangedCallback& callback) {
  on_state_changed_callbacks_.push_back(callback);
  callback.Run(*this);
}

void WebServClient::AddRequestHandler(const std::string& path_prefix,
                                      const OnRequestCallback& callback) {
  web_server_->GetDefaultHttpHandler()->AddHandlerCallback(
      path_prefix, "", base::Bind(&WebServClient::OnRequest,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  web_server_->GetDefaultHttpsHandler()->AddHandlerCallback(
      path_prefix, "", base::Bind(&WebServClient::OnRequest,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
}

uint16_t WebServClient::GetHttpPort() const {
  return http_port_;
}

uint16_t WebServClient::GetHttpsPort() const {
  return https_port_;
}

const chromeos::Blob& WebServClient::GetHttpsCertificateFingerprint() const {
  return certificate_;
}

WebServClient::WebServClient(
    const scoped_refptr<dbus::Bus>& bus,
    chromeos::dbus_utils::AsyncEventSequencer* sequencer) {
  web_server_.reset(new libwebserv::Server);
  web_server_->OnProtocolHandlerConnected(
      base::Bind(&WebServClient::OnProtocolHandlerConnected,
                 weak_ptr_factory_.GetWeakPtr()));
  web_server_->OnProtocolHandlerDisconnected(
      base::Bind(&WebServClient::OnProtocolHandlerDisconnected,
                 weak_ptr_factory_.GetWeakPtr()));

  web_server_->Connect(bus, buffet::kServiceName,
                       sequencer->GetHandler("Server::Connect failed.", true),
                       base::Bind(&base::DoNothing),
                       base::Bind(&base::DoNothing));
}

void WebServClient::OnRequest(const OnRequestCallback& callback,
                              std::unique_ptr<libwebserv::Request> request,
                              std::unique_ptr<libwebserv::Response> response) {
  callback.Run(
      RequestImpl{std::move(request)},
      base::Bind(&WebServClient::OnResponse, weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&response)));
}

void WebServClient::OnResponse(std::unique_ptr<libwebserv::Response> response,
                               int status_code,
                               const std::string& data,
                               const std::string& mime_type) {
  response->Reply(status_code, data.data(), data.size(), mime_type);
}

void WebServClient::OnProtocolHandlerConnected(
    libwebserv::ProtocolHandler* protocol_handler) {
  if (protocol_handler->GetName() == libwebserv::ProtocolHandler::kHttp) {
    http_port_ = *protocol_handler->GetPorts().begin();
  } else if (protocol_handler->GetName() ==
             libwebserv::ProtocolHandler::kHttps) {
    https_port_ = *protocol_handler->GetPorts().begin();
    certificate_ = protocol_handler->GetCertificateFingerprint();
  }
  for (const auto& cb : on_state_changed_callbacks_)
    cb.Run(*this);
}

void WebServClient::OnProtocolHandlerDisconnected(
    libwebserv::ProtocolHandler* protocol_handler) {
  if (protocol_handler->GetName() == libwebserv::ProtocolHandler::kHttp) {
    http_port_ = 0;
  } else if (protocol_handler->GetName() ==
             libwebserv::ProtocolHandler::kHttps) {
    https_port_ = 0;
    certificate_.clear();
  }
  for (const auto& cb : on_state_changed_callbacks_)
    cb.Run(*this);
}

}  // namespace buffet
