/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_ADAPTER_SCOPED_YUV_BUFFER_HANDLE_H_
#define CAMERA_HAL_ADAPTER_SCOPED_YUV_BUFFER_HANDLE_H_

#include <base/macros.h>

#include "cros-camera/camera_buffer_manager.h"

namespace cros {

class ScopedYUVBufferHandle {
 public:
  // Return wrapper of an existing YUV buffer handle. The scoped handle does
  // not own the buffer handle.
  static ScopedYUVBufferHandle CreateScopedYUVHandle(buffer_handle_t handle,
                                                     uint32_t width,
                                                     uint32_t height,
                                                     uint32_t flag);

  // Allocate a scoped YUV buffer handle.
  static ScopedYUVBufferHandle AllocateScopedYUVHandle(uint32_t width,
                                                       uint32_t height,
                                                       uint32_t flag);

  ScopedYUVBufferHandle(ScopedYUVBufferHandle&& other);

  ~ScopedYUVBufferHandle();

  operator bool() const;

  buffer_handle_t* GetHandle();

  // Lock and get YUV plane information. nullptr is returned on error.
  const struct android_ycbcr* LockYCbCr();

 private:
  ScopedYUVBufferHandle(buffer_handle_t handle,
                        bool takes_ownership,
                        uint32_t width,
                        uint32_t height,
                        uint32_t flag);

  buffer_handle_t handle_;

  bool owns_buffer_handle_;

  cros::CameraBufferManager* buffer_manager_;

  uint32_t width_;

  uint32_t height_;

  uint32_t flag_;

  struct android_ycbcr ycbcr_;

  ScopedYUVBufferHandle(const ScopedYUVBufferHandle&) = delete;
  ScopedYUVBufferHandle& operator=(const ScopedYUVBufferHandle&) = delete;
};

}  // namespace cros

#endif  // CAMERA_HAL_ADAPTER_SCOPED_YUV_BUFFER_HANDLE_H_
