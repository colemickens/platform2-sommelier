// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_SERVER_PROXY_H_
#define ARC_VM_VSOCK_PROXY_SERVER_PROXY_H_

#include <memory>

#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

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
  // Called when an initial VSOCK connection comes.
  // Then, accept()s it, connects to /run/chrome/arc_bridge.sock,
  // and registers it to vsock_proxy_ as an initial socket to be watched.
  void OnVSockReadReady();

  std::unique_ptr<VSockProxy> vsock_proxy_;
  base::ScopedFD vsock_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> vsock_controller_;

  base::WeakPtrFactory<ServerProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServerProxy);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_SERVER_PROXY_H_
