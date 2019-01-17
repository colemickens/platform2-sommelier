// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_CLIENT_PROXY_SERVICE_H_
#define ARC_VM_VSOCK_PROXY_CLIENT_PROXY_SERVICE_H_

#include <memory>

#include <base/macros.h>

namespace base {
class Thread;
class WaitableEvent;
}  // namespace base

namespace arc {

class ClientProxy;

// ClientProxyService is a service to run ClientProxy on a dedicated thread
// with MessageLoop.
class ClientProxyService {
 public:
  ClientProxyService();
  ~ClientProxyService();

  // Starts ClientProxy on a dedicated thread. This is blocked until the
  // ClientProxy's initialization is completed.
  // Returns true on success, otherwise false.
  bool Start();

  // Stops ClientProxy, and joins the dedicated thread.
  void Stop();

 private:
  void Initialize(base::WaitableEvent* event);
  void ShutDown();

  std::unique_ptr<base::Thread> thread_;

  // Proxy instance, which should be touched only on mThread.
  std::unique_ptr<ClientProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(ClientProxyService);
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_CLIENT_PROXY_SERVICE_H_
