// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_TEST_GRALLOC_H_
#define CAMERA3_TEST_CAMERA3_TEST_GRALLOC_H_

#include <memory>
#include <unordered_map>

#include <arc/camera_buffer_mapper.h>
#include <base/synchronization/lock.h>
#include <gbm.h>
#include <xf86drm.h>

#include "common/camera_buffer_handle.h"

namespace camera3_test {

struct BufferHandleDeleter {
  void operator()(buffer_handle_t* handle);
};

typedef std::unique_ptr<buffer_handle_t, struct BufferHandleDeleter>
    BufferHandleUniquePtr;

class Camera3TestGralloc {
 public:
  // Get Gralloc single instance
  static Camera3TestGralloc* GetInstance();

  // Allocate buffer by given parameters
  BufferHandleUniquePtr Allocate(int width, int height, int format, int usage);

  // This method is analogous to the lock() function in Android gralloc module.
  // Here the buffer handle is mapped with the given args.
  //
  // Args:
  //    |buffer|: The buffer handle to map.
  //    |flags|:  Currently omitted and is reserved for future use.
  //    |x|: The base x coordinate in pixels.
  //    |y|: The base y coordinate in pixels.
  //    |width|: The width in pixels of the area to map.
  //    |height|: The height in pixels of the area to map.
  //    |out_addr|: The mapped address.
  //
  // Returns:
  //    0 on success with |out_addr| set with the mapped address;
  //    -EINVAL on invalid buffer handle or invalid buffer format.
  int Lock(buffer_handle_t buffer,
           uint32_t flags,
           uint32_t x,
           uint32_t y,
           uint32_t width,
           uint32_t height,
           void** out_addr);

  // This method is analogous to the lock_ycbcr() function in Android gralloc
  // module.  Here all the physical planes of the buffer handle are mapped with
  // the given args.
  //
  // Args:
  //    |buffer|: The buffer handle to map.
  //    |flags|:  Currently omitted and is reserved for future use.
  //    |x|: The base x coordinate in pixels.
  //    |y|: The base y coordinate in pixels.
  //    |width|: The width in pixels of the area to map.
  //    |height|: The height in pixels of the area to map.
  //    |out_ycbcr|: The mapped addresses, plane strides and chroma offset.
  //        - |out_ycbcr.y| stores the mapped address of the Y-plane.
  //        - |out_ycbcr.cb| stores the mapped address of the Cb-plane.
  //        - |out_ycbcr.cr| stores the mapped address of the Cr-plane.
  //        - |out_ycbcr.ystride| stores the stride of the Y-plane.
  //        - |out_ycbcr.cstride| stores the stride of the chroma planes.
  //        - |out_ycbcr.chroma_step| stores the distance between two adjacent
  //          pixels on the chroma plane. The value is 1 for normal planar
  //          formats, and 2 for semi-planar formats.
  //
  // Returns:
  //    0 on success with |out_ycbcr.y| set with the mapped buffer info;
  //    -EINVAL on invalid buffer handle or invalid buffer format.
  int LockYCbCr(buffer_handle_t buffer,
                uint32_t flags,
                uint32_t x,
                uint32_t y,
                uint32_t width,
                uint32_t height,
                struct android_ycbcr* out_ycbcr);

  // This method is analogous to the unlock() function in Android gralloc
  // module.  Here the buffer is simply unmapped.
  //
  // Args:
  //    |buffer|: The buffer handle to unmap.
  //
  // Returns:
  //    0 on success; -EINVAL on invalid buffer handle.
  int Unlock(buffer_handle_t buffer);

  // Get buffer format
  // Returns:
  //    HAL_PIXEL_FORMAT_* on success; -EINVAL on invalid buffer handle.
  static int GetFormat(buffer_handle_t buffer);

  // Get V4L2 pixel format
  // Returns:
  //    V4L2 pixel format on success; 0 on failure.
  static uint32_t GetV4L2PixelFormat(buffer_handle_t buffer);

 private:
  // Free buffer
  int Free(buffer_handle_t handle);

  Camera3TestGralloc();

  ~Camera3TestGralloc();

  // Conversion from HAL to GBM usage flags
  uint64_t GrallocConvertFlags(int format, int flags);

  // Conversion from HAL to fourcc-based GBM formats
  uint32_t GrallocConvertFormat(int format);

  gbm_device* gbm_dev_;

  // Real GBM format of flexible YUV 420. The flexible format here does not
  // necessarily match the yuv420 format allocated by Android gralloc, but for
  // testing we are free to choose any yuv420 format that works.
  uint32_t flexible_yuv_420_format_;

  arc::CameraBufferMapper* buffer_mapper_;
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_TEST_GRALLOC_H_
