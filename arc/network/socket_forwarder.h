// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_SOCKET_FORWARDER_H_
#define ARC_NETWORK_SOCKET_FORWARDER_H_

#include <netinet/ip.h>
#include <sys/socket.h>

#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/threading/simple_thread.h>

#include "arc/network/socket.h"

namespace arc_networkd {

// Forwards data between a pair of sockets.
// This is a simple implementation as a thread main function.
class SocketForwarder : public base::SimpleThread {
 public:
  SocketForwarder(const std::string& name,
                  std::unique_ptr<Socket> sock0,
                  std::unique_ptr<Socket> sock1);
  virtual ~SocketForwarder();

  // Runs the forwarder. The sockets are closed and released on exit,
  // so this can only be run once.
  void Run() override;
  bool IsValid() const { return sock0_ && sock1_; }

 private:
  std::unique_ptr<Socket> sock0_;
  std::unique_ptr<Socket> sock1_;

  DISALLOW_COPY_AND_ASSIGN(SocketForwarder);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_SOCKET_FORWARDER_H_
