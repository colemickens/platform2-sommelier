// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_PROXY_SERVICE_H_
#define ARC_VM_VSOCK_PROXY_PROXY_SERVICE_H_

#include <memory>

#include <base/macros.h>
#include <base/memory/ref_counted.h>

namespace base {
class TaskRunner;
class Thread;
class WaitableEvent;
}  // namespace base

namespace arc {

class ProxyBase;

// ProxyService is a service to run a Proxy (practically ClientProxy or
// ServerProxy) on a dedicated thread with MessageLoop.
class ProxyService {
 public:
  explicit ProxyService(std::unique_ptr<ProxyBase> proxy);
  ~ProxyService();

  ProxyService(const ProxyService&) = delete;
  ProxyService& operator=(const ProxyService&) = delete;

  // Starts a Proxy on a dedicated thread. This is blocked until the Proxy's
  // initialization is completed. Returns true on success, otherwise false.
  bool Start();

  // Stops ClientProxy, and joins the dedicated thread.
  void Stop();

  // Returns the TaskRunner for the dedicated thread.
  // Must be called on the thread where this instance is created.
  scoped_refptr<base::TaskRunner> GetTaskRunner();

 private:
  void Initialize(base::WaitableEvent* event, bool* out);
  void ShutDown();

  std::unique_ptr<base::Thread> thread_;

  // Proxy instance, which should be touched only on |thread_|.
  std::unique_ptr<ProxyBase> proxy_;
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_PROXY_SERVICE_H_
