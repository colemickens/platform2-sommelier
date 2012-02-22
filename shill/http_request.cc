// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/http_request.h"

#include <string>

#include <base/logging.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>

#include "shill/async_connection.h"
#include "shill/connection.h"
#include "shill/dns_client.h"
#include "shill/event_dispatcher.h"
#include "shill/http_url.h"
#include "shill/ip_address.h"
#include "shill/sockets.h"

using base::StringPrintf;
using std::string;

namespace shill {

const int HTTPRequest::kConnectTimeoutSeconds = 10;
const int HTTPRequest::kDNSTimeoutSeconds = 5;
const int HTTPRequest::kInputTimeoutSeconds = 10;

const char HTTPRequest::kHTTPRequestTemplate[] =
    "GET %s HTTP/1.1\r\n"
    "Host: %s:%d\r\n"
    "Connection: Close\r\n\r\n";

HTTPRequest::HTTPRequest(ConnectionRefPtr connection,
                         EventDispatcher *dispatcher,
                         Sockets *sockets)
    : connection_(connection),
      dispatcher_(dispatcher),
      sockets_(sockets),
      connect_completion_callback_(
          NewCallback(this, &HTTPRequest::OnConnectCompletion)),
      dns_client_callback_(NewCallback(this, &HTTPRequest::GetDNSResult)),
      read_server_callback_(NewCallback(this, &HTTPRequest::ReadFromServer)),
      write_server_callback_(NewCallback(this, &HTTPRequest::WriteToServer)),
      result_callback_(NULL),
      read_event_callback_(NULL),
      task_factory_(this),
      idle_timeout_(NULL),
      dns_client_(
          new DNSClient(IPAddress::kFamilyIPv4,
                        connection->interface_name(),
                        connection->dns_servers(),
                        kDNSTimeoutSeconds * 1000,
                        dispatcher,
                        dns_client_callback_.get())),
      server_async_connection_(
          new AsyncConnection(connection_->interface_name(),
                              dispatcher_, sockets,
                              connect_completion_callback_.get())),
      server_port_(-1),
      server_socket_(-1),
      timeout_result_(kResultUnknown),
      is_running_(false) { }

HTTPRequest::~HTTPRequest() {
  Stop();
}

HTTPRequest::Result HTTPRequest::Start(
    const HTTPURL &url,
    Callback1<int>::Type *read_event_callback,
    Callback1<Result>::Type *result_callback) {
  VLOG(3) << "In " << __func__;

  DCHECK(!is_running_);

  is_running_ = true;
  request_data_ = ByteString(StringPrintf(kHTTPRequestTemplate,
                                          url.path().c_str(),
                                          url.host().c_str(),
                                          url.port()), false);
  server_hostname_ = url.host();
  server_port_ = url.port();
  connection_->RequestRouting();

  IPAddress addr(IPAddress::kFamilyIPv4);
  if (addr.SetAddressFromString(server_hostname_)) {
    if (!ConnectServer(addr, server_port_)) {
      LOG(ERROR) << "Connect to "
                 << server_hostname_
                 << " failed synchronously";
      return kResultConnectionFailure;
    }
  } else {
    VLOG(3) << "Looking up host: " << server_hostname_;
    if (!dns_client_->Start(server_hostname_)) {
      LOG(ERROR) << "Failed to start DNS client";
      Stop();
      return kResultDNSFailure;
    }
  }

  // Only install callbacks after connection succeeds in starting.
  read_event_callback_ = read_event_callback;
  result_callback_ = result_callback;

  return kResultInProgress;
}

void HTTPRequest::Stop() {
  VLOG(3) << "In " << __func__ << "; running is " << is_running_;

  if (!is_running_) {
    return;
  }

  // Clear IO handlers first so that closing the socket doesn't cause
  // events to fire.
  write_server_handler_.reset();
  read_server_handler_.reset();

  connection_->ReleaseRouting();
  dns_client_->Stop();
  idle_timeout_ = NULL;
  is_running_ = false;
  result_callback_ = NULL;
  read_event_callback_ = NULL;
  request_data_.Clear();
  response_data_.Clear();
  server_async_connection_->Stop();
  server_hostname_.clear();
  server_port_ = -1;
  if (server_socket_ != -1) {
    sockets_->Close(server_socket_);
    server_socket_ = -1;
  }
  task_factory_.RevokeAll();
  timeout_result_ = kResultUnknown;
}

bool HTTPRequest::ConnectServer(const IPAddress &address, int port) {
  VLOG(3) << "In " << __func__;
  if (!server_async_connection_->Start(address, port)) {
    LOG(ERROR) << "Could not create socket to connect to server at "
               << address.ToString();
    SendStatus(kResultConnectionFailure);
    return false;
  }
  // Start a connection timeout only if we didn't synchronously connect.
  if (server_socket_ == -1) {
    StartIdleTimeout(kConnectTimeoutSeconds, kResultConnectionTimeout);
  }
  return true;
}

// DNSClient callback that fires when the DNS request completes.
void HTTPRequest::GetDNSResult(bool result) {
  VLOG(3) << "In " << __func__;
  if (!result) {
    const string &error = dns_client_->error();
    LOG(ERROR) << "Could not resolve hostname "
               << server_hostname_
               << ": "
               << error;
    if (error == DNSClient::kErrorTimedOut) {
      SendStatus(kResultDNSTimeout);
    } else {
      SendStatus(kResultDNSFailure);
    }
    return;
  }
  ConnectServer(dns_client_->address(), server_port_);
}

// AsyncConnection callback routine which fires when the asynchronous Connect()
// to the remote server completes (or fails).
void HTTPRequest::OnConnectCompletion(bool success, int fd) {
  VLOG(3) << "In " << __func__;
  if (!success) {
    LOG(ERROR) << "Socket connection delayed failure to "
               << server_hostname_
               << ": "
               << server_async_connection_->error();
    SendStatus(kResultConnectionFailure);
    return;
  }
  server_socket_ = fd;
  write_server_handler_.reset(
      dispatcher_->CreateReadyHandler(server_socket_,
                                      IOHandler::kModeOutput,
                                      write_server_callback_.get()));
  StartIdleTimeout(kInputTimeoutSeconds, kResultRequestTimeout);
}

// IOInputHandler callback which fires when data has been read from the
// server.
void HTTPRequest::ReadFromServer(InputData *data) {
  VLOG(3) << "In " << __func__ << " length " << data->len;
  if (data->len == 0) {
    SendStatus(kResultSuccess);
    return;
  }

  response_data_.Append(ByteString(data->buf, data->len));
  if (read_event_callback_) {
    read_event_callback_->Run(data->len);
  }
  StartIdleTimeout(kInputTimeoutSeconds, kResultResponseTimeout);
}

void HTTPRequest::SendStatus(Result result) {
  if (result_callback_) {
    result_callback_->Run(result);
  }
  Stop();
}

// Start a timeout for "the next event".
void HTTPRequest::StartIdleTimeout(int timeout_seconds, Result timeout_result) {
  if (idle_timeout_) {
    idle_timeout_->Cancel();
  }
  timeout_result_ = timeout_result;
  idle_timeout_ = task_factory_.NewRunnableMethod(&HTTPRequest::TimeoutTask);
  dispatcher_->PostDelayedTask(idle_timeout_, timeout_seconds * 1000);
}

void HTTPRequest::TimeoutTask() {
  LOG(ERROR) << "Connection with "
             << server_hostname_
             << " timed out";
  SendStatus(timeout_result_);
}

// Output ReadyHandler callback which fires when the server socket is
// ready for data to be sent to it.
void HTTPRequest::WriteToServer(int fd) {
  CHECK_EQ(server_socket_, fd);
  int ret = sockets_->Send(fd, request_data_.GetConstData(),
                           request_data_.GetLength(), 0);
  CHECK(static_cast<size_t>(ret) <= request_data_.GetLength());

  VLOG(3) << "In " << __func__ << " wrote " << ret << " of " <<
      request_data_.GetLength();

  if (ret < 0) {
    LOG(ERROR) << "Client write failed to "
               << server_hostname_;
    SendStatus(kResultRequestFailure);
    return;
  }

  request_data_ = ByteString(request_data_.GetConstData() + ret,
                             request_data_.GetLength() - ret);

  if (request_data_.IsEmpty()) {
    write_server_handler_->Stop();
    read_server_handler_.reset(
        dispatcher_->CreateInputHandler(server_socket_,
                                        read_server_callback_.get()));
    StartIdleTimeout(kInputTimeoutSeconds, kResultResponseTimeout);
  } else {
    StartIdleTimeout(kInputTimeoutSeconds, kResultRequestTimeout);
  }
}

}  // namespace shill
