// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_PROXY_BASE_H_
#define ARC_VM_VSOCK_PROXY_PROXY_BASE_H_

namespace arc {

class VSockProxy;

// Interface of ServerProxy and ClientProxy.
class ProxyBase {
 public:
  virtual ~ProxyBase() = default;

  // Initializes the instance. Returns true on success.
  virtual bool Initialize() = 0;

  // Returns VSockProxy instance.
  virtual VSockProxy* GetVSockProxy() = 0;
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_PROXY_BASE_H_
