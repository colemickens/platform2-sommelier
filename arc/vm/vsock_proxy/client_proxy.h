// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_CLIENT_PROXY_H_
#define ARC_VM_VSOCK_PROXY_CLIENT_PROXY_H_

#include <stdint.h>

#include <memory>

#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

namespace arc {

class VSockProxy;

// ClientProxy sets up the VSockProxy and handles initial socket negotiation.
class ClientProxy {
 public:
  ClientProxy();
  ~ClientProxy();

  // Sets up the ClientProxy. Specifically, wait for VSOCK gets ready,
  // creates a unix domain socket at /var/run/chrome/arc_bridge.sock,
  // then starts watching it.
  void Initialize();

 private:
  // Called when /var/run/chrome/arc_bridge.sock gets ready to read.
  // Then, accept()s and registers it to mVSockProxy as an initial socket
  // to be watched.
  void OnLocalSocketReadReady();

  // Called when host-side connect(2) is completed.
  void OnConnected(int error_code, int64_t handle);

  std::unique_ptr<VSockProxy> vsock_proxy_;
  base::ScopedFD arc_bridge_socket_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller>
      arc_bridge_socket_controller_;

  base::WeakPtrFactory<ClientProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ClientProxy);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_CLIENT_PROXY_H_
