// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_SERVER_PROXY_H_
#define ARC_VM_VSOCK_PROXY_SERVER_PROXY_H_

#include <memory>

#include <base/macros.h>

namespace arc {

class VSockProxy;

// ServerProxy sets up the VSockProxy and handles initial socket negotiation.
class ServerProxy {
 public:
  ServerProxy();
  ~ServerProxy();

  // Sets up the ServerProxy. Specifically, start listening VSOCK.
  // Then, connect to /run/chrome/arc_bridge.sock, when an initial connection
  // comes to the vsock.
  void Initialize();

 private:
  std::unique_ptr<VSockProxy> vsock_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ServerProxy);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_SERVER_PROXY_H_
