// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_test/camera3_test_gralloc.h"

#include <drm_fourcc.h>
#include <linux/videodev2.h>

#include <algorithm>

#include <arc/camera_buffer_manager.h>
#include <base/files/file_util.h>

namespace camera3_test {

void BufferHandleDeleter::operator()(buffer_handle_t* handle) {
  if (handle) {
    auto* cbm = arc::CameraBufferManager::GetInstance();
    if (cbm) {
      cbm->Free(*handle);
    }
    delete handle;
  }
}

base::Lock Camera3TestGralloc::lock_;

// static
Camera3TestGralloc* Camera3TestGralloc::GetInstance() {
  static std::unique_ptr<Camera3TestGralloc> gralloc;
  base::AutoLock l(lock_);
  if (!gralloc) {
    gralloc.reset(new Camera3TestGralloc());
    if (!gralloc->Initialize()) {
      gralloc.reset();
    }
  }
  return gralloc.get();
}

Camera3TestGralloc::Camera3TestGralloc()
    : buffer_manager_(arc::CameraBufferManager::GetInstance()) {}

bool Camera3TestGralloc::Initialize() {
  if (!buffer_manager_) {
    LOG(ERROR) << "Failed to get buffer mapper";
    return false;
  }
  return true;
}

BufferHandleUniquePtr Camera3TestGralloc::Allocate(int32_t width,
                                                   int32_t height,
                                                   int32_t format,
                                                   int32_t usage) {
  if (!buffer_manager_) {
    return BufferHandleUniquePtr(nullptr);
  }
  BufferHandleUniquePtr handle(new buffer_handle_t);
  uint32_t stride;
  if (buffer_manager_->Allocate(width, height, format, usage, arc::GRALLOC,
                                handle.get(), &stride)) {
    return BufferHandleUniquePtr(nullptr);
  }
  return handle;
}

int Camera3TestGralloc::Lock(buffer_handle_t buffer,
                             uint32_t flags,
                             uint32_t x,
                             uint32_t y,
                             uint32_t width,
                             uint32_t height,
                             void** out_addr) {
  return buffer_manager_->Lock(buffer, flags, x, y, width, height, out_addr);
}

int Camera3TestGralloc::LockYCbCr(buffer_handle_t buffer,
                                  uint32_t flags,
                                  uint32_t x,
                                  uint32_t y,
                                  uint32_t width,
                                  uint32_t height,
                                  struct android_ycbcr* out_ycbcr) {
  return buffer_manager_->LockYCbCr(buffer, flags, x, y, width, height,
                                    out_ycbcr);
}

int Camera3TestGralloc::Unlock(buffer_handle_t buffer) {
  return buffer_manager_->Unlock(buffer);
}

// static
int Camera3TestGralloc::GetFormat(buffer_handle_t buffer) {
  auto hnd = camera_buffer_handle_t::FromBufferHandle(buffer);
  return (hnd && hnd->buffer_id) ? hnd->hal_pixel_format : -EINVAL;
}

// static
uint32_t Camera3TestGralloc::GetV4L2PixelFormat(buffer_handle_t buffer) {
  return arc::CameraBufferManager::GetInstance()->GetV4L2PixelFormat(buffer);
}

}  // namespace camera3_test
