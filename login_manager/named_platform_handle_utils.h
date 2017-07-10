// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(yusukes): Uprev libmojo to >=r415560 and remove this file. Recent
// libmojo has CreateServerHandle().

#ifndef LOGIN_MANAGER_NAMED_PLATFORM_HANDLE_UTILS_H_
#define LOGIN_MANAGER_NAMED_PLATFORM_HANDLE_UTILS_H_

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"

namespace login_manager {

typedef int PlatformHandle;

// Copied from Chromium r485157's mojo/edk/embedder/named_platform_handle.h.
struct NamedPlatformHandle {
  NamedPlatformHandle() {}
  explicit NamedPlatformHandle(const base::StringPiece& name)
      : name(name.as_string()) {}

  bool is_valid() const { return !name.empty(); }

  std::string name;
};

// Copied from Chromium r485157's mojo/edk/embedder/scoped_platform_handle.h.
class ScopedPlatformHandle {
 public:
  ScopedPlatformHandle() : handle_(-1) {}
  explicit ScopedPlatformHandle(PlatformHandle handle) : handle_(handle) {}
  ~ScopedPlatformHandle() {
    if (handle_ != -1)
      close(handle_);
  }

  // Move-only constructor and operator=.
  ScopedPlatformHandle(ScopedPlatformHandle&& other)
      : handle_(other.release()) {}

  ScopedPlatformHandle& operator=(ScopedPlatformHandle&& other) {
    if (this != &other)
      handle_ = other.release();
    return *this;
  }

  const PlatformHandle& get() const { return handle_; }

  void swap(ScopedPlatformHandle& other) {
    PlatformHandle temp = handle_;
    handle_ = other.handle_;
    other.handle_ = temp;
  }

  PlatformHandle release() WARN_UNUSED_RESULT {
    PlatformHandle rv = handle_;
    handle_ = -1;
    return rv;
  }

  void reset(PlatformHandle handle = -1) {
    if (handle_ != -1)
      close(handle_);
    handle_ = handle;
  }

  bool is_valid() const { return handle_ != -1; }

 private:
  PlatformHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPlatformHandle);
};

// Creates a server platform handle from |named_handle|.
ScopedPlatformHandle CreateServerHandle(
    const NamedPlatformHandle& named_handle);

}  // namespace login_manager

#endif  // LOGIN_MANAGER_NAMED_PLATFORM_HANDLE_UTILS_H_
