// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/shared_memory_provider.h"

namespace mri {

SharedMemoryProvider::SharedMemoryProvider(
    bool read_only, mojo::edk::ScopedPlatformHandle* scoped_handle,
    uint32_t memory_size_in_bytes) {
  // TODO(fqj): migrate to new SharedMemoryHandle(fd, size, guid);
  base::SharedMemoryHandle memory_handle(scoped_handle->release().handle, true);
  mapped_size_ = memory_size_in_bytes;
  shared_memory_.reset(new base::SharedMemory(memory_handle, read_only));
}

SharedMemoryProvider::~SharedMemoryProvider() {
  if (shared_memory_ && shared_memory_->memory()) {
    LOG(INFO) << __func__ << ": Unmapping memory for in-process access @"
              << shared_memory_->memory() << '.';
    CHECK(shared_memory_->Unmap());
  }
}

std::unique_ptr<SharedMemoryProvider>
SharedMemoryProvider::CreateFromRawFileDescriptor(
    bool read_only, mojo::ScopedHandle fd_handle,
    uint32_t memory_size_in_bytes) {
  mojo::edk::ScopedPlatformHandle scoped_platform_handle;
  MojoResult mojo_result = mojo::edk::PassWrappedPlatformHandle(
      fd_handle.release().value(), &scoped_platform_handle);
  if (mojo_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "Failed to unwrap handle: " << mojo_result;
    return nullptr;
  }
  return std::unique_ptr<SharedMemoryProvider>(new SharedMemoryProvider(
      read_only, &scoped_platform_handle, memory_size_in_bytes));
}

uint32_t SharedMemoryProvider::GetMemorySizeInBytes() {
  return static_cast<uint32_t>(mapped_size_);
}

base::SharedMemory* SharedMemoryProvider::GetSharedMemoryForInProcessAccess() {
  if (!shared_memory_->memory()) {
    CHECK(shared_memory_->Map(mapped_size_));
    LOG(INFO) << __func__ << ": Mapped memory for in-process access @"
              << shared_memory_->memory() << '.';
  }
  return shared_memory_.get();
}

}  // namespace mri
