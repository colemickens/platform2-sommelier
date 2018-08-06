/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/reprocess_effect/scoped_buffer_handle.h"

#include <utility>

#include "cros-camera/common.h"

namespace cros {

// static
ScopedYUVBufferHandle ScopedYUVBufferHandle::CreateScopedHandle(
    buffer_handle_t handle, uint32_t flag, uint32_t width, uint32_t height) {
  ScopedYUVBufferHandle scoped_handle;
  if (scoped_handle.buffer_manager_->Register(handle) != 0) {
    LOGF(ERROR) << "Failed to register buffer handle";
    return scoped_handle;
  }
  android_ycbcr ycbcr = {};
  if (scoped_handle.buffer_manager_->LockYCbCr(handle, flag, 0, 0, width,
                                               height, &ycbcr) != 0) {
    LOGF(ERROR) << "Failed to lock buffer handle";
    scoped_handle.buffer_manager_->Deregister(handle);
    return scoped_handle;
  }
  scoped_handle.handle_ = handle;
  scoped_handle.ycbcr_ = ycbcr;
  return scoped_handle;
}

ScopedYUVBufferHandle::ScopedYUVBufferHandle()
    : handle_(nullptr),
      buffer_manager_(cros::CameraBufferManager::GetInstance()),
      ycbcr_{} {}

ScopedYUVBufferHandle::ScopedYUVBufferHandle(ScopedYUVBufferHandle&& other)
    : handle_(other.handle_),
      buffer_manager_(cros::CameraBufferManager::GetInstance()),
      ycbcr_(other.ycbcr_) {
  other.handle_ = nullptr;
  other.buffer_manager_ = buffer_manager_;
  other.ycbcr_ = {};
}

ScopedYUVBufferHandle::~ScopedYUVBufferHandle() {
  if (handle_) {
    buffer_manager_->Unlock(handle_);
    buffer_manager_->Deregister(handle_);
  }
}

ScopedYUVBufferHandle::operator bool() const {
  return (ycbcr_.y != nullptr);
}

const struct android_ycbcr* ScopedYUVBufferHandle::GetYCbCr() {
  return &ycbcr_;
}

}  // namespace cros
