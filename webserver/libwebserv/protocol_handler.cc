// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webserver/libwebserv/protocol_handler.h"

#include <tuple>

#include <base/logging.h>
#include <chromeos/map_utils.h>

#include "libwebserv/org.chromium.WebServer.RequestHandler.h"
#include "webservd/dbus-proxies.h"
#include "webserver/libwebserv/request.h"
#include "webserver/libwebserv/request_handler_callback.h"
#include "webserver/libwebserv/response.h"
#include "webserver/libwebserv/server.h"

namespace libwebserv {

namespace {

// A dummy callback for async D-Bus errors.
void IgnoreError(chromeos::Error* error) {}

}  // anonymous namespace

const char ProtocolHandler::kHttp[] = "http";
const char ProtocolHandler::kHttps[] = "https";

ProtocolHandler::ProtocolHandler(const std::string& id, Server* server)
    : id_{id}, server_{server} {}

ProtocolHandler::~ProtocolHandler() {
  // Remove any existing handlers, so the web server knows that we don't
  // need them anymore.

  // We need to get a copy of the map keys since removing the handlers will
  // modify the map in the middle of the loop and that's not a good thing.
  auto handler_ids = chromeos::GetMapKeys(request_handlers_);
  for (int handler_id : handler_ids) {
    RemoveHandler(handler_id);
  }
}

std::string ProtocolHandler::GetID() const {
  return id_;
}

uint16_t ProtocolHandler::GetPort() const {
  CHECK(IsConnected());
  return proxy_->port();
}

std::string ProtocolHandler::GetProtocol() const {
  CHECK(IsConnected());
  return proxy_->protocol();
}

chromeos::Blob ProtocolHandler::GetCertificateFingerprint() const {
  CHECK(IsConnected());
  return proxy_->certificate_fingerprint();
}

int ProtocolHandler::AddHandler(
    const std::string& url,
    const std::string& method,
    std::unique_ptr<RequestHandlerInterface> handler) {
  request_handlers_.emplace(++last_handler_id_,
                            HandlerMapEntry{url, method, std::string{},
                                            std::move(handler)});
  if (proxy_) {
    proxy_->AddRequestHandlerAsync(
        url,
        method,
        server_->service_name_,
        base::Bind(&ProtocolHandler::AddHandlerSuccess,
                   weak_ptr_factory_.GetWeakPtr(),
                   last_handler_id_),
        base::Bind(&ProtocolHandler::AddHandlerError,
                   weak_ptr_factory_.GetWeakPtr(),
                   last_handler_id_));
  }
  return last_handler_id_;
}

int ProtocolHandler::AddHandlerCallback(
    const std::string& url,
    const std::string& method,
    const base::Callback<RequestHandlerInterface::HandlerSignature>&
        handler_callback) {
  std::unique_ptr<RequestHandlerInterface> handler{
      new RequestHandlerCallback{handler_callback}};
  return AddHandler(url, method, std::move(handler));
}

bool ProtocolHandler::RemoveHandler(int handler_id) {
  auto p = request_handlers_.find(handler_id);
  if (p == request_handlers_.end())
    return false;

  if (proxy_) {
    proxy_->RemoveRequestHandlerAsync(
        p->second.remote_handler_id,
        base::Bind(&base::DoNothing),
        base::Bind(&IgnoreError));
  }

  request_handlers_.erase(p);
  return true;
}

void ProtocolHandler::Connect(
    org::chromium::WebServer::ProtocolHandlerProxy* proxy) {
  proxy_ = proxy;
  for (const auto& pair : request_handlers_) {
    proxy_->AddRequestHandlerAsync(
        pair.second.url,
        pair.second.method,
        server_->service_name_,
        base::Bind(&ProtocolHandler::AddHandlerSuccess,
                   weak_ptr_factory_.GetWeakPtr(),
                   pair.first),
        base::Bind(&ProtocolHandler::AddHandlerError,
                   weak_ptr_factory_.GetWeakPtr(),
                   pair.first));
  }
}

void ProtocolHandler::Disconnect() {
  proxy_ = nullptr;
  remote_handler_id_map_.clear();
}

void ProtocolHandler::AddHandlerSuccess(int handler_id,
                                        const std::string& remote_handler_id) {
  auto p = request_handlers_.find(handler_id);
  CHECK(p != request_handlers_.end());
  p->second.remote_handler_id = remote_handler_id;

  remote_handler_id_map_.emplace(remote_handler_id, handler_id);
}

void ProtocolHandler::AddHandlerError(int handler_id, chromeos::Error* error) {
  // Nothing to do at the moment.
}

bool ProtocolHandler::ProcessRequest(const std::string& remote_handler_id,
                                     const std::string& request_id,
                                     std::unique_ptr<Request> request,
                                     chromeos::ErrorPtr* error) {
  auto id_iter = remote_handler_id_map_.find(remote_handler_id);
  if (id_iter == remote_handler_id_map_.end()) {
    chromeos::Error::AddToPrintf(error, FROM_HERE,
                                 chromeos::errors::dbus::kDomain,
                                 DBUS_ERROR_FAILED,
                                 "Unknown request handler '%s'",
                                 remote_handler_id.c_str());
    return false;
  }
  auto handler_iter = request_handlers_.find(id_iter->second);
  if (handler_iter == request_handlers_.end()) {
    chromeos::Error::AddToPrintf(error, FROM_HERE,
                                 chromeos::errors::dbus::kDomain,
                                 DBUS_ERROR_FAILED,
                                 "Handler # %d is no longer available",
                                 id_iter->second);
    return false;
  }
  handler_iter->second.handler->HandleRequest(
      scoped_ptr<Request>(request.release()),
      scoped_ptr<Response>(new Response{this, request_id}));
  return true;
}

void ProtocolHandler::CompleteRequest(
    const std::string& request_id,
    int status_code,
    const std::multimap<std::string, std::string>& headers,
    const std::vector<uint8_t>& data) {
  if (!proxy_) {
    LOG(WARNING) << "Completing a request after the handler proxy is removed";
    return;
  }

  std::vector<std::tuple<std::string, std::string>> header_list;
  header_list.reserve(headers.size());
  for (const auto& pair : headers)
    header_list.emplace_back(pair.first, pair.second);

  proxy_->CompleteRequestAsync(request_id, status_code, header_list, data,
                               base::Bind(&base::DoNothing),
                               base::Bind([](chromeos::Error*) {}));
}

void ProtocolHandler::GetFileData(
    const std::string& request_id,
    int file_id,
    const base::Callback<void(const std::vector<uint8_t>&)>& success_callback,
    const base::Callback<void(chromeos::Error*)>& error_callback) {
  CHECK(proxy_);
  proxy_->GetRequestFileDataAsync(
      request_id, file_id, success_callback, error_callback);
}


}  // namespace libwebserv
