// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "diagnostics/common/mojo_utils.h"

#include <unistd.h>

#include <cstring>
#include <utility>

#include <base/files/file.h>
#include <base/memory/shared_memory_handle.h>
#include <mojo/public/c/system/types.h>
#include <mojo/public/cpp/system/platform_handle.h>

namespace diagnostics {

std::unique_ptr<base::SharedMemory> GetReadOnlySharedMemoryFromMojoHandle(
    mojo::ScopedHandle handle) {
  base::PlatformFile platform_file;
  auto result = mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (result != MOJO_RESULT_OK) {
    return nullptr;
  }

  const int fd(HANDLE_EINTR(dup(platform_file)));
  if (fd < 0) {
    return nullptr;
  }

  const int64_t file_size = base::File(fd).GetLength();
  if (file_size <= 0) {
    return nullptr;
  }

  auto shared_memory = std::make_unique<base::SharedMemory>(
      base::SharedMemoryHandle(
          base::FileDescriptor(platform_file, true /* iauto_close */),
          file_size, base::UnguessableToken::Create()),
      true /* read_only */);

  if (!shared_memory->Map(file_size)) {
    return nullptr;
  }
  return shared_memory;
}

mojo::ScopedHandle CreateReadOnlySharedMemoryMojoHandle(
    base::StringPiece content) {
  if (content.empty())
    return mojo::ScopedHandle();
  base::SharedMemory shared_memory;
  base::SharedMemoryCreateOptions options;
  options.size = content.length();
  options.share_read_only = true;
  if (!shared_memory.Create(base::SharedMemoryCreateOptions(options)) ||
      !shared_memory.Map(content.length())) {
    return mojo::ScopedHandle();
  }
  memcpy(shared_memory.memory(), content.data(), content.length());
  base::SharedMemoryHandle handle = shared_memory.GetReadOnlyHandle();
  if (!handle.IsValid()) {
    return mojo::ScopedHandle();
  }
  return mojo::WrapPlatformFile(handle.GetHandle());
}

}  // namespace diagnostics
