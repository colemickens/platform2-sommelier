// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/diagnosticsd/mojo_utils.h"

#include <unistd.h>
#include <cstring>
#include <utility>

#include <base/memory/shared_memory_handle.h>
#include <base/posix/eintr_wrapper.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/edk/embedder/scoped_platform_handle.h>
#include <mojo/public/c/system/types.h>

namespace diagnostics {

namespace {

mojo::ScopedHandle WrapPlatformHandle(const base::SharedMemoryHandle& handle) {
  MojoHandle wrapped_handle;
  mojo::edk::PlatformHandle platform_handle(HANDLE_EINTR(dup(handle.fd)));
  MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
      mojo::edk::ScopedPlatformHandle(platform_handle), &wrapped_handle);
  if (wrap_result != MOJO_RESULT_OK) {
    return mojo::ScopedHandle();
  }
  return mojo::ScopedHandle(mojo::Handle(wrapped_handle));
}

}  // namespace

std::unique_ptr<base::SharedMemory> GetReadOnlySharedMemoryFromMojoHandle(
    mojo::ScopedHandle handle, int64_t size) {
  mojo::edk::ScopedPlatformHandle platform_handle;
  auto result = mojo::edk::PassWrappedPlatformHandle(handle.release().value(),
                                                     &platform_handle);
  if (result != MOJO_RESULT_OK) {
    return nullptr;
  }

  base::SharedMemoryHandle shared_memory_handle(
      platform_handle.release().handle, true /* iauto_close */);

  std::unique_ptr<base::SharedMemory> shared_memory =
      std::make_unique<base::SharedMemory>(std::move(shared_memory_handle),
                                           true /* read_only */);
  if (!shared_memory->Map(size)) {
    return nullptr;
  }
  return shared_memory;
}

mojo::ScopedHandle CreateReadOnlySharedMemoryMojoHandle(
    base::StringPiece content) {
  base::SharedMemory shared_memory;
  base::SharedMemoryCreateOptions options;
  options.size = content.length();
  options.share_read_only = true;
  if (!shared_memory.Create(base::SharedMemoryCreateOptions(options)) ||
      !shared_memory.Map(content.length())) {
    return mojo::ScopedHandle();
  }
  memcpy(shared_memory.memory(), content.data(), content.length());
  return WrapPlatformHandle(shared_memory.handle());
}

}  // namespace diagnostics
