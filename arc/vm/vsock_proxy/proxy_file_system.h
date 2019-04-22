// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_PROXY_FILE_SYSTEM_H_
#define ARC_VM_VSOCK_PROXY_PROXY_FILE_SYSTEM_H_

#include <stdint.h>

#include <base/files/scoped_file.h>

namespace arc {

// Interface to regular file descriptor passing over VSOCK.
// As for ServerProxyFileSystem, this interface helps to split the dependency
// from FUSE.
class ProxyFileSystem {
 public:
  virtual ~ProxyFileSystem() = default;

  // Registers the given |handle| to the file system, then returns the file
  // descriptor corresponding to the registered file.
  // The operation for the returned file descriptor will behave as if it is
  // somehow done in the other side. E.g., read for the returned file
  // descriptor will behave as if the file descriptor for the |handle| in
  // the other side is read.
  virtual base::ScopedFD RegisterHandle(int64_t handle) = 0;
};

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_PROXY_FILE_SYSTEM_H_
