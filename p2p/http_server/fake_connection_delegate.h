// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_HTTP_SERVER_FAKE_CONNECTION_DELEGATE_H__
#define P2P_HTTP_SERVER_FAKE_CONNECTION_DELEGATE_H__

#include "http_server/connection_delegate_interface.h"
#include "http_server/server_interface.h"

namespace p2p {

namespace http_server {

// An implementation of ConnectionDelegateInterface that doesn't actually
// serve any data.
class FakeConnectionDelegate : public ConnectionDelegateInterface {
 public:
  FakeConnectionDelegate(
      int dirfd,
      int fd,
      const std::string& pretty_addr,
      ServerInterface* server,
      int64_t max_download_rate) : fd_(fd), server_(server) {}

  // A ConnectionDelegate factory.
  static ConnectionDelegateInterface* Construct(
      int dirfd,
      int fd,
      const std::string& pretty_addr,
      ServerInterface* server,
      int64_t max_download_rate) {
    return new FakeConnectionDelegate(dirfd, fd, pretty_addr, server,
        max_download_rate);
  }

  // Overrides ConnectionDelegateInterface.
  virtual void Run() {
    server_->ConnectionTerminated(this);
    close(fd_);
  }

 private:
  int fd_;
  ServerInterface* server_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionDelegate);
};

}  // namespace http_server

}  // namespace p2p

#endif  // P2P_HTTP_SERVER_FAKE_CONNECTION_DELEGATE_H__
