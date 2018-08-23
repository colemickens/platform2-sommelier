/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/scoped_yuv_buffer_handle.h"

#include <utility>

#include "cros-camera/common.h"

namespace cros {

ScopedYUVBufferHandle::ScopedYUVBufferHandle(ScopedYUVBufferHandle&& other)
    : handle_(other.handle_),
      owns_buffer_handle_(other.owns_buffer_handle_),
      buffer_manager_(cros::CameraBufferManager::GetInstance()),
      width_(other.width_),
      height_(other.height_),
      flag_(other.flag_),
      ycbcr_(other.ycbcr_) {
  other.handle_ = nullptr;
  other.owns_buffer_handle_ = false;
  other.width_ = 0;
  other.height_ = 0;
  other.flag_ = 0;
  other.ycbcr_ = {};
}

// static
ScopedYUVBufferHandle ScopedYUVBufferHandle::CreateScopedYUVHandle(
    buffer_handle_t handle, uint32_t width, uint32_t height, uint32_t flag) {
  if (cros::CameraBufferManager::GetInstance()->Register(handle) != 0) {
    LOGF(ERROR) << "Failed to register buffer handle";
    return ScopedYUVBufferHandle(nullptr, false, 0, 0, 0);
  }
  return ScopedYUVBufferHandle(handle, false, width, height, flag);
}

// static
ScopedYUVBufferHandle ScopedYUVBufferHandle::AllocateScopedYUVHandle(
    uint32_t width, uint32_t height, uint32_t flag) {
  buffer_handle_t handle;
  uint32_t stride;
  if (cros::CameraBufferManager::GetInstance()->Allocate(
          width, height, HAL_PIXEL_FORMAT_YCbCr_420_888, flag, cros::GRALLOC,
          &handle, &stride) != 0) {
    LOGF(ERROR) << "Failed to allocate buffer handle";
    return ScopedYUVBufferHandle(nullptr, false, 0, 0, 0);
  }
  return ScopedYUVBufferHandle(handle, true, width, height, flag);
}

ScopedYUVBufferHandle::~ScopedYUVBufferHandle() {
  if (ycbcr_.y != nullptr) {
    buffer_manager_->Unlock(handle_);
  }
  if (handle_ != nullptr) {
    if (!owns_buffer_handle_) {
      buffer_manager_->Deregister(handle_);
    } else {
      buffer_manager_->Free(handle_);
    }
  }
}

ScopedYUVBufferHandle::operator bool() const {
  return (handle_ != nullptr);
}

buffer_handle_t* ScopedYUVBufferHandle::GetHandle() {
  return &handle_;
}

const struct android_ycbcr* ScopedYUVBufferHandle::LockYCbCr() {
  if (!ycbcr_.y) {
    android_ycbcr ycbcr = {};
    if (buffer_manager_->LockYCbCr(handle_, flag_, 0, 0, width_, height_,
                                   &ycbcr) != 0) {
      LOGF(ERROR) << "Failed to lock buffer handle";
      buffer_manager_->Deregister(handle_);
      return nullptr;
    }
    ycbcr_ = ycbcr;
  }
  return &ycbcr_;
}

ScopedYUVBufferHandle::ScopedYUVBufferHandle(buffer_handle_t handle,
                                             bool takes_ownership,
                                             uint32_t width,
                                             uint32_t height,
                                             uint32_t flag)
    : handle_(handle),
      owns_buffer_handle_(takes_ownership),
      buffer_manager_(cros::CameraBufferManager::GetInstance()),
      width_(width),
      height_(height),
      flag_(flag),
      ycbcr_{} {}

}  // namespace cros
