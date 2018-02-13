// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_SERVER_FAKE_HTTP_SERVER_H_
#define P2P_SERVER_FAKE_HTTP_SERVER_H_

#include <string>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>

#include "p2p/server/http_server.h"

namespace p2p {

namespace server {

// A HTTP server that doesn't really serve any files and can be made
// to lie about its number of connected clients.
class FakeHttpServer : public HttpServer {
 public:
  FakeHttpServer() : is_running_(false), num_connections_(0) {}

  virtual bool Start() {
    is_running_ = true;
    return true;
  }

  virtual bool Stop() {
    is_running_ = false;
    return true;
  }

  virtual bool IsRunning() { return is_running_; }

  virtual uint16_t Port() { return 1234; }

  virtual void SetNumConnectionsCallback(NumConnectionsCallback callback) {
    callback_ = callback;
  }

  void SetNumConnections(int num_connections) {
    if (num_connections_ != num_connections) {
      num_connections_ = num_connections;
      if (!callback_.is_null())
        callback_.Run(num_connections);
    }
  }

 private:
  bool is_running_;
  NumConnectionsCallback callback_;
  int num_connections_;

  DISALLOW_COPY_AND_ASSIGN(FakeHttpServer);
};

}  // namespace server

}  // namespace p2p

#endif  // P2P_SERVER_FAKE_HTTP_SERVER_H_
