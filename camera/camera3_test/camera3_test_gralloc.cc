// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_test/camera3_test_gralloc.h"

#include <drm_fourcc.h>
#include <linux/videodev2.h>

#include <algorithm>

#include <arc/camera_buffer_mapper.h>
#include <base/files/file_util.h>

#include "common/camera_buffer_mapper_internal.h"

namespace camera3_test {

void BufferHandleDeleter::operator()(buffer_handle_t* handle) {
  if (handle) {
    auto hnd = camera_buffer_handle_t::FromBufferHandle(*handle);
    if (hnd) {
      if (hnd->buffer_id) {
        arc::CameraBufferMapper::GetInstance()->Deregister(*handle);
        gbm_bo_destroy(reinterpret_cast<struct gbm_bo*>(hnd->buffer_id));
      } else {
        LOG(ERROR) << "Buffer handle mapping fails";
      }
      delete hnd;
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
    : buffer_mapper_(arc::CameraBufferMapper::GetInstance()) {}

bool Camera3TestGralloc::Initialize() {
  if (!buffer_mapper_) {
    LOG(ERROR) << "Failed to get buffer mapper";
    return false;
  }
  gbm_dev_ = buffer_mapper_->GetGbmDevice();
  if (!gbm_dev_) {
    LOG(ERROR) << "Failed to get gbm device";
    return false;
  }

  uint32_t formats[] = {DRM_FORMAT_YVU420, DRM_FORMAT_NV12};
  size_t i = 0;
  for (; i < arraysize(formats); i++) {
    if (gbm_dev_->IsFormatSupported(formats[i], GBM_BO_USE_LINEAR |
                                                    GBM_BO_USE_CAMERA_READ |
                                                    GBM_BO_USE_CAMERA_WRITE)) {
      flexible_yuv_420_format_ = formats[i];
      break;
    }
  }
  if (i == arraysize(formats)) {
    LOG(ERROR) << "Can't detect flexible YUV 420 format";
    return false;
  }
  return true;
}

BufferHandleUniquePtr Camera3TestGralloc::Allocate(int32_t width,
                                                   int32_t height,
                                                   int32_t format,
                                                   int32_t usage) {
  DCHECK(gbm_dev_);
  uint64_t gbm_usage;
  uint32_t gbm_format;
  struct gbm_bo* bo;

  gbm_format = GrallocConvertFormat(format);
  gbm_usage = GrallocConvertFlags(format, usage);

  if (!gbm_dev_) {
    return BufferHandleUniquePtr(nullptr);
  }
  if (gbm_format == 0 || !gbm_dev_->IsFormatSupported(gbm_format, gbm_usage)) {
    LOG(ERROR) << "Unsupported format " << std::hex << gbm_format;
    return BufferHandleUniquePtr(nullptr);
  }

  bo = gbm_dev_->CreateBo(width, height, gbm_format, gbm_usage);
  if (!bo) {
    LOG(ERROR) << "Failed to create bo (" << width << "x" << height << ")";
    return BufferHandleUniquePtr(nullptr);
  }

  camera_buffer_handle_t* hnd = new camera_buffer_handle_t();

  hnd->base.version = sizeof(hnd->base);
  hnd->base.numInts = kCameraBufferHandleNumInts;
  hnd->base.numFds = kCameraBufferHandleNumFds;

  hnd->magic = kCameraBufferMagic;
  hnd->buffer_id = reinterpret_cast<uint64_t>(bo);
  hnd->type = arc::GRALLOC;
  hnd->drm_format = gbm_bo_get_format(bo);
  hnd->hal_pixel_format = format;
  hnd->width = gbm_bo_get_width(bo);
  hnd->height = gbm_bo_get_height(bo);
  for (size_t i = 0; i < gbm_bo_get_num_planes(bo); ++i) {
    hnd->fds[i].reset(gbm_bo_get_plane_fd(bo, i));
    hnd->strides[i] = gbm_bo_get_plane_stride(bo, i);
    hnd->offsets[i] = gbm_bo_get_plane_offset(bo, i);
  }

  BufferHandleUniquePtr handle(new buffer_handle_t);
  *handle = reinterpret_cast<buffer_handle_t>(hnd);
  buffer_mapper_->Register(*handle);
  return handle;
}

int Camera3TestGralloc::Lock(buffer_handle_t buffer,
                             uint32_t flags,
                             uint32_t x,
                             uint32_t y,
                             uint32_t width,
                             uint32_t height,
                             void** out_addr) {
  return buffer_mapper_->Lock(buffer, flags, x, y, width, height, out_addr);
}

int Camera3TestGralloc::LockYCbCr(buffer_handle_t buffer,
                                  uint32_t flags,
                                  uint32_t x,
                                  uint32_t y,
                                  uint32_t width,
                                  uint32_t height,
                                  struct android_ycbcr* out_ycbcr) {
  return buffer_mapper_->LockYCbCr(buffer, flags, x, y, width, height,
                                   out_ycbcr);
}

int Camera3TestGralloc::Unlock(buffer_handle_t buffer) {
  return buffer_mapper_->Unlock(buffer);
}

// static
int Camera3TestGralloc::GetFormat(buffer_handle_t buffer) {
  auto hnd = camera_buffer_handle_t::FromBufferHandle(buffer);
  return (hnd && hnd->buffer_id) ? hnd->hal_pixel_format : -EINVAL;
}

// static
uint32_t Camera3TestGralloc::GetV4L2PixelFormat(buffer_handle_t buffer) {
  return arc::CameraBufferMapper::GetInstance()->GetV4L2PixelFormat(buffer);
}

uint64_t Camera3TestGralloc::GrallocConvertFlags(int32_t format,
                                                 int32_t flags) {
  return (format == HAL_PIXEL_FORMAT_BLOB)
             ? GBM_BO_USE_LINEAR
             : GBM_BO_USE_LINEAR | GBM_BO_USE_CAMERA_READ |
                   GBM_BO_USE_CAMERA_WRITE;
}

uint32_t Camera3TestGralloc::GrallocConvertFormat(int32_t format) {
  switch (format) {
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
      return flexible_yuv_420_format_;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_YCbCr_420_888:
      return flexible_yuv_420_format_;
    case HAL_PIXEL_FORMAT_YV12:
      return DRM_FORMAT_YVU420;
    case HAL_PIXEL_FORMAT_BLOB:
      return DRM_FORMAT_R8;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace camera3_test
