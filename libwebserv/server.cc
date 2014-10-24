// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libwebserv/server.h"

#include <limits>

#include <base/logging.h>
#include <base/message_loop/message_loop_proxy.h>
#include <microhttpd.h>

#include "libwebserv/connection.h"
#include "libwebserv/request.h"
#include "libwebserv/request_handler_callback.h"
#include "libwebserv/request_handler_interface.h"
#include "libwebserv/response.h"

namespace libwebserv {

namespace {
// Simple static request handler that just returns "404 Not Found" error.
class PageNotFoundHandler : public RequestHandlerInterface {
 public:
  void HandleRequest(const std::shared_ptr<const Request>& request,
                     const std::shared_ptr<Response>& response) override {
    response->ReplyWithErrorNotFound();
  }
};
}  // anonymous namespace

// Helper class to provide static callback methods to microhttpd library,
// with the ability to access private methods of Server class.
class ServerHelper {
 public:
  static int ConnectionHandler(void *cls,
                               MHD_Connection* connection,
                               const char* url,
                               const char* method,
                               const char* version,
                               const char* upload_data,
                               size_t* upload_data_size,
                               void** con_cls) {
    Server* server = reinterpret_cast<Server*>(cls);
    if (nullptr == *con_cls) {
      RequestHandlerInterface* handler = server->FindHandler(url, method);
      if (!handler)
        return MHD_NO;

      auto server_connection =
          Connection::Create(server, url, method, connection, handler);
      if (!server_connection || !server_connection->BeginRequestData())
        return MHD_NO;

      *con_cls = new ConnectionHolder(server_connection);
    } else {
      ConnectionHolder* holder = reinterpret_cast<ConnectionHolder*>(*con_cls);

      if (*upload_data_size) {
        if (!holder->connection->AddRequestData(upload_data, *upload_data_size))
          return MHD_NO;
        *upload_data_size = 0;
      } else {
        holder->connection->EndRequestData();
      }
    }
    return MHD_YES;
  }

  static void RequestCompleted(void* cls,
                               MHD_Connection* connection,
                               void** con_cls,
                               MHD_RequestTerminationCode toe) {
    delete reinterpret_cast<ConnectionHolder*>(*con_cls);
    *con_cls = nullptr;
  }

 private:
  class ConnectionHolder {
   public:
    explicit ConnectionHolder(const std::shared_ptr<Connection>& conn)
        : connection(conn) {}
    std::shared_ptr<Connection> connection;
  };
};

Server::Server() {}

Server::~Server() {
  Stop();
}

bool Server::Start(uint16_t port) {
  if (server_) {
    LOG(ERROR) << "Server already running.";
    return false;
  }

  task_runner_ = base::MessageLoopProxy::current();

  LOG(INFO) << "Starting HTTP Server on port: " << port;
  MHD_OptionItem options[] = {
    {MHD_OPTION_CONNECTION_LIMIT, 10, nullptr},
    {MHD_OPTION_CONNECTION_TIMEOUT, 10, nullptr},
    {MHD_OPTION_NOTIFY_COMPLETED,
        reinterpret_cast<intptr_t>(&ServerHelper::RequestCompleted),
                                   nullptr},
    {MHD_OPTION_END, 0, nullptr }
  };

  server_ = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
                             port,
                             nullptr, nullptr,
                             &ServerHelper::ConnectionHandler, this,
                             MHD_OPTION_ARRAY, options,
                             MHD_OPTION_END);
  if (!server_) {
    LOG(ERROR) << "Failed to start the web server on port " << port;
    return false;
  }
  LOG(INFO) << "Server started";
  return true;
}

bool Server::Stop() {
  if (server_) {
    LOG(INFO) << "Shutting down the HTTP server...";
    MHD_stop_daemon(server_);
    server_ = nullptr;
    LOG(INFO) << "Server shutdown complete";
  }
  return true;
}

int Server::AddHandler(const base::StringPiece& url,
                       const base::StringPiece& method,
                       std::unique_ptr<RequestHandlerInterface> handler) {
  request_handlers_.emplace(++last_handler_id_,
                            HandlerMapEntry{url.as_string(),
                                            method.as_string(),
                                            std::move(handler)});
  return last_handler_id_;
}

int Server::AddHandlerCallback(
    const base::StringPiece& url,
    const base::StringPiece& method,
    const base::Callback<RequestHandlerInterface::HandlerSignature>&
        handler_callback) {
  std::unique_ptr<RequestHandlerInterface> handler{
      new RequestHandlerCallback{handler_callback}};
  return AddHandler(url, method, std::move(handler));
}

bool Server::RemoveHandler(int handler_id) {
  return (request_handlers_.erase(handler_id) > 0);
}

int Server::GetHandlerId(const base::StringPiece& url,
                         const base::StringPiece& method) const {
  for (const auto& pair : request_handlers_) {
    if (pair.second.url == url && pair.second.method == method)
      return pair.first;
  }
  return 0;
}

RequestHandlerInterface* Server::FindHandler(
    const base::StringPiece& url,
    const base::StringPiece& method) const {
  static PageNotFoundHandler page_not_found_handler;

  size_t score = std::numeric_limits<size_t>::max();
  RequestHandlerInterface* handler = nullptr;
  for (const auto& pair : request_handlers_) {
    std::string handler_url = pair.second.url;
    bool url_match = (handler_url == url);
    bool method_match = (pair.second.method == method);

    // Try exact match first. If everything matches, we have our handler.
    if (url_match && method_match)
      return pair.second.handler.get();

    // Calculate the current handler's similarity score. The lower the score
    // the better the match is...
    size_t current_score = 0;
    if (!url_match && !handler_url.empty() && handler_url.back() == '/') {
      if (url.starts_with(handler_url)) {
        url_match = true;
        // Use the difference in URL length as URL match quality proxy.
        // The longer URL, the more specific (better) match is.
        // Multiply by 2 to allow for extra score point for matching the method.
        current_score = (url.size() - handler_url.size()) * 2;
      }
    }

    if (!method_match && pair.second.method.empty()) {
      // If the handler didn't specify the method it handles, this means
      // it doesn't care. However this isn't the exact match, so bump
      // the score up one point.
      method_match = true;
      ++current_score;
    }

    if (url_match && method_match && current_score < score) {
      score = current_score;
      handler = pair.second.handler.get();
    }
  }

  return handler ? handler : &page_not_found_handler;
}

}  // namespace libwebserv
