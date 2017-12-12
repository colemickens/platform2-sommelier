// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VM_TOOLS_CONCIERGE_VSOCK_CID_POOL_H_
#define VM_TOOLS_CONCIERGE_VSOCK_CID_POOL_H_

#include <stdint.h>

#include <base/macros.h>

namespace vm_tools {
namespace concierge {

// Manages a pool of virtual socket context IDs to be assigned to VMs.
class VsockCidPool {
 public:
  VsockCidPool() = default;
  ~VsockCidPool() = default;

  // Allocates and returns a vsock context id.
  uint32_t Allocate() { return next_cid_++; }

 private:
  // The next context id to hand out.  Cids 0 and 1 are reserved while cid 2 is
  // always the host system.  Guest cids start at 3.
  uint32_t next_cid_{3};

  DISALLOW_COPY_AND_ASSIGN(VsockCidPool);
};

}  // namespace concierge
}  // namespace vm_tools

#endif  // VM_TOOLS_CONCIERGE_VSOCK_CID_POOL_H_
