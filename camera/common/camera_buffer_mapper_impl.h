/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COMMON_CAMERA_BUFFER_MAPPER_IMPL_H_
#define COMMON_CAMERA_BUFFER_MAPPER_IMPL_H_

#include "arc/camera_buffer_mapper.h"

#include <memory>

#include <base/synchronization/lock.h>

#include "common/camera_buffer_mapper_typedefs.h"

// A V4L2 extension format which represents 32bit RGBX-8-8-8-8 format. This
// corresponds to DRM_FORMAT_XBGR8888 which is used as the underlying format for
// the HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINEND format on all CrOS boards.
#define V4L2_PIX_FMT_RGBX32 v4l2_fourcc('X', 'B', '2', '4')

namespace arc {

namespace tests {

class CameraBufferMapperImplTest;

}  // namespace tests

class EXPORTED CameraBufferMapperImpl final : public CameraBufferMapper {
 public:
  CameraBufferMapperImpl();

  // CameraBufferMapper implementation.
  ~CameraBufferMapperImpl() final;
  int Allocate(size_t width,
               size_t height,
               uint32_t format,
               uint32_t usage,
               BufferType type,
               buffer_handle_t* out_buffer,
               uint32_t* out_stride) final;
  int Free(buffer_handle_t buffer) final;
  int Register(buffer_handle_t buffer) final;
  int Deregister(buffer_handle_t buffer) final;
  int Lock(buffer_handle_t buffer,
           uint32_t flags,
           uint32_t x,
           uint32_t y,
           uint32_t width,
           uint32_t height,
           void** out_addr) final;
  int LockYCbCr(buffer_handle_t buffer,
                uint32_t flags,
                uint32_t x,
                uint32_t y,
                uint32_t width,
                uint32_t height,
                struct android_ycbcr* out_ycbcr) final;
  int Unlock(buffer_handle_t buffer) final;
  GbmDevice* GetGbmDevice() final;

 private:
  friend class CameraBufferMapper;

  // Allow unit tests to call constructor directly.
  friend class tests::CameraBufferMapperImplTest;

  // Revoles the HAL pixel format |hal_format| to the actual DRM format, based
  // on the gralloc usage flags set in |usage|.
  uint32_t ResolveFormat(uint32_t hal_format, uint32_t usage);

  int AllocateGrallocBuffer(size_t width,
                            size_t height,
                            uint32_t format,
                            uint32_t usage,
                            buffer_handle_t* out_buffer,
                            uint32_t* out_stride);

  int AllocateShmBuffer(size_t width,
                        size_t height,
                        uint32_t format,
                        uint32_t usage,
                        buffer_handle_t* out_buffer,
                        uint32_t* out_stride);

  // Maps |buffer| and returns the mapped address.
  //
  // Args:
  //    |buffer|: The buffer handle to map.
  //    |flags|:  Currently omitted and is reserved for future use.
  //    |plane|: The plane to map.
  //
  // Returns:
  //    The mapped address on success; MAP_FAILED on failure.
  void* Map(buffer_handle_t buffer, uint32_t flags, uint32_t plane);

  // Unmaps |buffer|.
  //
  // Args:
  //    |buffer|: The buffer handle to unmap.
  //    |plane|: The plane to unmap.
  //
  // Returns:
  //    0 on success; -EINVAL if |buffer| is invalid.
  int Unmap(buffer_handle_t buffer, uint32_t plane);

  // Lock to guard access member variables.
  base::Lock lock_;

  // ** Start of lock_ scope **

  // The handle to the opened GBM device.
  std::unique_ptr<GbmDevice, GbmDevice::Deleter> gbm_device_;

  // A cache which stores all the context of the registered buffers.
  // For gralloc buffers the context stores the imported GBM buffer objects.
  // For shm buffers the context stores the mapped address and the buffer size.
  // |buffer_context_| needs to be placed before |buffer_info_| to make sure the
  // GBM buffer objects are valid when we unmap them in |buffer_info_|'s
  // destructor.
  BufferContextCache buffer_context_;

  // The private info about all the mapped (buffer, plane) pairs.
  // |buffer_info_| has to be placed after |gbm_device_| so that the GBM device
  // is still valid when we delete the MappedGrallocBufferInfoUniquePtr.
  // This is only used by gralloc buffers.
  MappedGrallocBufferInfoCache buffer_info_;

  // ** End of lock_ scope **

  DISALLOW_COPY_AND_ASSIGN(CameraBufferMapperImpl);
};

}  // namespace arc

#endif  // COMMON_CAMERA_BUFFER_MAPPER_IMPL_H_
