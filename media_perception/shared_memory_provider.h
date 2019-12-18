// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_SHARED_MEMORY_PROVIDER_H_
#define MEDIA_PERCEPTION_SHARED_MEMORY_PROVIDER_H_

#include <base/files/scoped_file.h>
#include <base/memory/shared_memory.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <memory>
// NOLINTNEXTLINE
#include "base/logging.h"

namespace mri {

// Wrapper on base::SharedMemory to provide init from file descriptor
// functionality and mapping and unmapping for in-process access.
class SharedMemoryProvider {
 public:
  ~SharedMemoryProvider();

  // Returns a new SharedMemoryProvider unique_ptr, which is a nullptr if the
  // initialization failed and sets up |shared_memory_| from a file descriptor.
  static std::unique_ptr<SharedMemoryProvider> CreateFromRawFileDescriptor(
      bool read_only,
      mojo::ScopedHandle fd_handle,
      uint32_t memory_size_in_bytes);

  // Shared memory is owned by the provider.
  base::SharedMemory* GetSharedMemoryForInProcessAccess();

  uint32_t GetMemorySizeInBytes();

 private:
  // Initializes the |shared_memory_| member.
  // scoped_handle is owned by the caller.
  SharedMemoryProvider(bool read_only,
                       base::ScopedFD scoped_handle,
                       uint32_t memory_size_in_bytes);

  std::unique_ptr<base::SharedMemory> shared_memory_;
  size_t mapped_size_;
};

}  // namespace mri

#endif  // MEDIA_PERCEPTION_SHARED_MEMORY_PROVIDER_H_
