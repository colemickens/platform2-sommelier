// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/async_connection.h"

#include <base/bind.h>
#include <errno.h>
#include <netinet/in.h>

#include <string>

#include "shill/event_dispatcher.h"
#include "shill/ip_address.h"
#include "shill/sockets.h"

using base::Bind;
using base::Callback;
using base::Unretained;
using std::string;

namespace shill {

AsyncConnection::AsyncConnection(const string &interface_name,
                                 EventDispatcher *dispatcher,
                                 Sockets *sockets,
                                 const Callback<void(bool, int)> &callback)
    : interface_name_(interface_name),
      dispatcher_(dispatcher),
      sockets_(sockets),
      callback_(callback),
      fd_(-1),
      connect_completion_callback_(
          Bind(&AsyncConnection::OnConnectCompletion, Unretained(this))) { }

AsyncConnection::~AsyncConnection() {
  Stop();
}

bool AsyncConnection::Start(const IPAddress &address, int port) {
  DCHECK(fd_ < 0);

  fd_ = sockets_->Socket(PF_INET, SOCK_STREAM, 0);
  if (fd_ < 0 ||
      sockets_->SetNonBlocking(fd_) < 0) {
    error_ = sockets_->ErrorString();
    PLOG(ERROR) << "Async socket setup failed";
    Stop();
    return false;
  }

  if (!interface_name_.empty() &&
      sockets_->BindToDevice(fd_, interface_name_) < 0) {
    error_ = sockets_->ErrorString();
    PLOG(ERROR) << "Async socket failed to bind to device";
    Stop();
    return false;
  }

  struct sockaddr_in iaddr;
  CHECK_EQ(sizeof(iaddr.sin_addr.s_addr), address.GetLength());

  memset(&iaddr, 0, sizeof(iaddr));
  iaddr.sin_family = AF_INET;
  memcpy(&iaddr.sin_addr.s_addr, address.address().GetConstData(),
         sizeof(iaddr.sin_addr.s_addr));
  iaddr.sin_port = htons(port);

  socklen_t addrlen = sizeof(iaddr);
  int ret = sockets_->Connect(fd_,
                              reinterpret_cast<struct sockaddr *>(&iaddr),
                              addrlen);
  if (ret == 0) {
    callback_.Run(true, fd_);  // Passes ownership
    fd_ = -1;
    return true;
  }

  if (sockets_->Error() != EINPROGRESS) {
    error_ = sockets_->ErrorString();
    PLOG(ERROR) << "Async socket connection failed";
    Stop();
    return false;
  }

  connect_completion_handler_.reset(
      dispatcher_->CreateReadyHandler(fd_,
                                      IOHandler::kModeOutput,
                                      connect_completion_callback_));
  error_ = string();

  return true;
}

void AsyncConnection::Stop() {
  connect_completion_handler_.reset();
  if (fd_ >= 0) {
    sockets_->Close(fd_);
    fd_ = -1;
  }
}

void AsyncConnection::OnConnectCompletion(int fd) {
  CHECK_EQ(fd_, fd);

  if (sockets_->GetSocketError(fd_) != 0) {
    error_ = sockets_->ErrorString();
    PLOG(ERROR) << "Async GetSocketError returns failure";
    callback_.Run(false, -1);
  } else {
    callback_.Run(true, fd_);  // Passes ownership
    fd_ = -1;
  }
  Stop();
}

}  // namespace shill
