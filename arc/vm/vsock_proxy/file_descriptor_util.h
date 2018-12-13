// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_VM_VSOCK_PROXY_FILE_DESCRIPTOR_UTIL_H_
#define ARC_VM_VSOCK_PROXY_FILE_DESCRIPTOR_UTIL_H_

#include <utility>

#include <base/optional.h>
#include <base/files/scoped_file.h>

namespace arc {

// Creates a pair of pipe file descriptors, and returns it.
// Returns nullopt if failed.
base::Optional<std::pair<base::ScopedFD, base::ScopedFD>> CreatePipe();

// Creates a pair of socketpair file desciprotrs, and returns it.
// Returns nullopt if failed.
base::Optional<std::pair<base::ScopedFD, base::ScopedFD>> CreateSocketPair();

}  // namespace arc

#endif  // ARC_VM_VSOCK_PROXY_FILE_DESCRIPTOR_UTIL_H_
