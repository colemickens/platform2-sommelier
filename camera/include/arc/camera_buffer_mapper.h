/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef INCLUDE_ARC_CAMERA_BUFFER_MAPPER_H_
#define INCLUDE_ARC_CAMERA_BUFFER_MAPPER_H_

#include <base/synchronization/lock.h>

#include "arc/camera_buffer_mapper_typedefs.h"

#define EXPORTED __attribute__((__visibility__("default")))

// A V4L2 extension format which represents 32bit RGBX-8-8-8-8 format. This
// corresponds to DRM_FORMAT_XBGR8888 which is used as the underlying format for
// the HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINEND format on all CrOS boards.
#define V4L2_PIX_FMT_RGBX32 v4l2_fourcc('X', 'B', '2', '4')

namespace arc {

namespace tests {

class CameraBufferMapperTest;

}  // namespace tests

// Generic camera buffer mapper.  The class is for a camera HAL to map and unmap
// the buffer handles received in camera3_stream_buffer_t.
//
// The class is thread-safe.
//
// Example usage:
//
//  #include <arc/camera_buffer_mapper.h>
//  CameraBufferMapper* mapper = CameraBufferMapper::GetInstance();
//  if (!mapper) {
//    /* Error handling */
//  }
//  mapper->Register(buffer_handle);
//  void* addr;
//  mapper->Lock(buffer_handle, ..., &addr);
//  /* Access the buffer mapped to |addr| */
//  mapper->Unlock(buffer_handle);
//  mapper->Deregister(buffer_handle);

class EXPORTED CameraBufferMapper {
 public:
  // Gets the singleton instance.  Returns nullptr if any error occurrs during
  // instance creation.
  static CameraBufferMapper* GetInstance();

  // This method is analogous to the register() function in Android gralloc
  // module.  This method needs to be called before |buffer| can be mapped.
  //
  // Args:
  //    |buffer|: The buffer handle to register.
  //
  // Returns:
  //    0 on success; corresponding error code on failure.
  int Register(buffer_handle_t buffer);

  // This method is analogous to the unregister() function in Android gralloc
  // module.  After |buffer| is deregistered, calling Lock(), LockYCbCr(), or
  // Unlock() on |buffer| will fail.
  //
  // Args:
  //    |buffer|: The buffer handle to deregister.
  //
  // Returns:
  //    0 on success; corresponding error code on failure.
  int Deregister(buffer_handle_t buffer);

  // This method is analogous to the lock() function in Android gralloc module.
  // Here the buffer handle is mapped with the given args.
  //
  // This method always maps the entire buffer and |x|, |y|, |width|, |height|
  // do not affect |out_addr|.
  //
  // Args:
  //    |buffer|: The buffer handle to map.
  //    |flags|:  Currently omitted and is reserved for future use.
  //    |x|: Unused and has no effect.
  //    |y|: Unused and has no effect.
  //    |width|: Unused and has no effect.
  //    |height|: Unused and has no effect.
  //    |out_addr|: The mapped address pointing to the start of the buffer.
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
  // This method always maps the entire buffer and |x|, |y|, |width|, |height|
  // do not affect |out_ycbcr|.
  //
  // Args:
  //    |buffer|: The buffer handle to map.
  //    |flags|:  Currently omitted and is reserved for future use.
  //    |x|: Unused and has no effect.
  //    |y|: Unused and has no effect.
  //    |width|: Unused and has no effect.
  //    |height|: Unused and has no effect.
  //    |out_ycbcr|: The mapped addresses, plane strides and chroma offset.
  //        - |out_ycbcr.y| stores the mapped address to the start of the
  //          Y-plane.
  //        - |out_ycbcr.cb| stores the mapped address to the start of the
  //          Cb-plane.
  //        - |out_ycbcr.cr| stores the mapped address to the start of the
  //          Cr-plane.
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

  // Get the number of physical planes associated with |buffer|.
  //
  // Args:
  //    |buffer|: The buffer handle to query.
  //
  // Returns:
  //    Number of planes on success; 0 if |buffer| is invalid or unrecognized
  //    pixel format.
  static uint32_t GetNumPlanes(buffer_handle_t buffer);

  // Gets the V4L2 pixel format for the buffer handle.
  //
  // Args:
  //    |buffer|: The buffer handle to query.
  //
  // Returns:
  //    The V4L2 pixel format; 0 on error.
  static uint32_t GetV4L2PixelFormat(buffer_handle_t buffer);

  // Gets the stride of the specified plane.
  //
  // Args:
  //    |buffer|: The buffer handle to query.
  //    |plane|: The plane to query.
  //
  // Returns:
  //    The stride of the specified plane; 0 on error.
  static size_t GetPlaneStride(buffer_handle_t buffer, size_t plane);

  // Gets the size of the specified plane.
  //
  // Args:
  //    |buffer|: The buffer handle to query.
  //    |plane|: The plane to query.
  //
  // Returns:
  //    The size of the specified plane; 0 on error.
  static size_t GetPlaneSize(buffer_handle_t buffer, size_t plane);

 private:
  // Allow unit tests to call constructor directly.
  friend class tests::CameraBufferMapperTest;

  CameraBufferMapper();

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

  // Lock to guard access member variables..
  base::Lock lock_;

  // ** Start of lock_ scope **

  // The handle to the opened GBM device.
  GbmDeviceUniquePtr gbm_device_;

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

  DISALLOW_COPY_AND_ASSIGN(CameraBufferMapper);
};

}  // namespace arc

#endif  // INCLUDE_ARC_CAMERA_BUFFER_MAPPER_H_
