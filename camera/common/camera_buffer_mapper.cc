/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "arc/camera_buffer_mapper.h"

#include <vector>

#include <linux/videodev2.h>
#include <sys/mman.h>

#include <drm_fourcc.h>
#include <gbm.h>

#include "arc/common.h"
#include "common/camera_buffer_handle.h"
#include "common/camera_buffer_mapper_internal.h"
#include "system/graphics.h"

namespace arc {

// static
CameraBufferMapper* CameraBufferMapper::GetInstance() {
  static CameraBufferMapper instance;
  if (!instance.gbm_device_) {
    LOGF(ERROR) << "Failed to create GBM device for CameraBufferMapper";
    return nullptr;
  }
  return &instance;
}

CameraBufferMapper::CameraBufferMapper()
    : gbm_device_(internal::CreateGbmDevice()) {}

int CameraBufferMapper::Register(buffer_handle_t buffer) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return -EINVAL;
  }

  base::AutoLock l(lock_);

  if (handle->type == GRALLOC) {
    auto it = buffer_context_.find(buffer);
    if (it == buffer_context_.end()) {
      BufferContextUniquePtr buffer_context(new struct BufferContext);
      // Import the buffer if we haven't done so.
      struct gbm_import_fd_planar_data import_data;
      memset(&import_data, 0, sizeof(import_data));
      import_data.width = handle->width;
      import_data.height = handle->height;
      import_data.format = handle->drm_format;
      uint32_t num_planes = GetNumPlanes(buffer);
      if (num_planes <= 0) {
        return -EINVAL;
      }
      for (size_t i = 0; i < num_planes; ++i) {
        import_data.fds[i] = handle->fds[i].get();
        import_data.strides[i] = handle->strides[i];
        import_data.offsets[i] = handle->offsets[i];
      }

      uint32_t usage = GBM_BO_USE_RENDERING;
      if (import_data.format == DRM_FORMAT_R8) {
        usage = GBM_BO_USE_LINEAR;
      }
      buffer_context->bo = gbm_bo_import(
          gbm_device_.get(), GBM_BO_IMPORT_FD_PLANAR, &import_data, usage);
      if (!buffer_context->bo) {
        LOGF(ERROR) << "Failed to import buffer 0x" << std::hex
                    << handle->buffer_id;
        return -EIO;
      }
      buffer_context->usage = 1;
      buffer_context_[buffer] = std::move(buffer_context);
    } else {
      it->second->usage++;
    }
    return 0;
  } else if (handle->type == SHM) {
    // The shared memory buffer is a contiguous area of memory which is large
    // enough to hold all the physical planes.  We mmap the buffer on Register
    // and munmap on Deregister.
    auto it = buffer_context_.find(buffer);
    if (it == buffer_context_.end()) {
      BufferContextUniquePtr buffer_context(new struct BufferContext);
      off_t size = lseek(handle->fds[0].get(), 0, SEEK_END);
      if (size == -1) {
        LOGF(ERROR) << "Failed to get shm buffer size through lseek: "
                    << strerror(errno);
        return -errno;
      }
      buffer_context->shm_buffer_size = static_cast<uint32_t>(size);
      lseek(handle->fds[0].get(), 0, SEEK_SET);
      buffer_context->mapped_addr =
          mmap(nullptr, buffer_context->shm_buffer_size, PROT_READ | PROT_WRITE,
               MAP_SHARED, handle->fds[0].get(), 0);
      if (buffer_context->mapped_addr == MAP_FAILED) {
        LOGF(ERROR) << "Failed to mmap shm buffer: " << strerror(errno);
        return -errno;
      }
      buffer_context->usage = 1;
      buffer_context_[buffer] = std::move(buffer_context);
    } else {
      it->second->usage++;
    }
    return 0;
  } else {
    NOTREACHED() << "Invalid buffer type: " << handle->type;
    return -EINVAL;
  }
}

int CameraBufferMapper::Deregister(buffer_handle_t buffer) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return -EINVAL;
  }

  base::AutoLock l(lock_);

  auto it = buffer_context_.find(buffer);
  if (it == buffer_context_.end()) {
    LOGF(ERROR) << "Unknown buffer 0x" << std::hex << handle->buffer_id;
    return -EINVAL;
  }
  auto buffer_context = it->second.get();
  if (handle->type == GRALLOC) {
    if (!--buffer_context->usage) {
      // Unmap all the existing mapping of bo.
      for (auto it = buffer_info_.begin(); it != buffer_info_.end();) {
        if (it->second->bo == it->second->bo) {
          it = buffer_info_.erase(it);
        } else {
          ++it;
        }
      }
      buffer_context_.erase(it);
    }
    return 0;
  } else if (handle->type == SHM) {
    if (!--buffer_context->usage) {
      int ret =
          munmap(buffer_context->mapped_addr, buffer_context->shm_buffer_size);
      if (ret == -1) {
        LOGF(ERROR) << "Failed to munmap shm buffer: " << strerror(errno);
      }
      buffer_context_.erase(it);
    }
    return 0;
  } else {
    NOTREACHED() << "Invalid buffer type: " << handle->type;
    return -EINVAL;
  }
}

int CameraBufferMapper::Lock(buffer_handle_t buffer,
                             uint32_t flags,
                             uint32_t x,
                             uint32_t y,
                             uint32_t width,
                             uint32_t height,
                             void** out_addr) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return -EINVAL;
  }
  uint32_t num_planes = GetNumPlanes(buffer);
  if (!num_planes) {
    return -EINVAL;
  }
  if (num_planes > 1) {
    LOGF(ERROR) << "Lock called on multi-planar buffer 0x" << std::hex
                << handle->buffer_id;
    return -EINVAL;
  }

  *out_addr = Map(buffer, flags, x, y, width, height, 0);
  if (*out_addr == MAP_FAILED) {
    return -EINVAL;
  }
  return 0;
}

int CameraBufferMapper::LockYCbCr(buffer_handle_t buffer,
                                  uint32_t flags,
                                  uint32_t x,
                                  uint32_t y,
                                  uint32_t width,
                                  uint32_t height,
                                  struct android_ycbcr* out_ycbcr) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return -EINVAL;
  }
  uint32_t num_planes = GetNumPlanes(buffer);
  if (!num_planes) {
    return -EINVAL;
  }
  if (num_planes < 2) {
    LOGF(ERROR) << "Lock called on single-planar buffer 0x" << std::hex
                << handle->buffer_id;
    return -EINVAL;
  }

  DCHECK_LE(num_planes, 3u);
  std::vector<uint8_t*> addr(3);
  for (size_t i = 0; i < num_planes; ++i) {
    void* a = Map(buffer, flags, x, y, width, height, i);
    if (a == MAP_FAILED) {
      return -EINVAL;
    }
    addr[i] = reinterpret_cast<uint8_t*>(a);
  }
  out_ycbcr->y = addr[0];
  out_ycbcr->ystride = handle->strides[0];
  out_ycbcr->cstride = handle->strides[1];

  if (num_planes == 2) {
    out_ycbcr->chroma_step = 2;
    switch (handle->drm_format) {
      case DRM_FORMAT_NV12:
        out_ycbcr->cb = addr[1] + handle->offsets[1] + 1;
        out_ycbcr->cr = addr[1] + handle->offsets[1];
        break;

      case DRM_FORMAT_NV21:
        out_ycbcr->cb = addr[1] + handle->offsets[1];
        out_ycbcr->cr = addr[1] + handle->offsets[1] + 1;
        break;

      default:
        LOGF(ERROR) << "Unsupported semi-planar format: "
                    << FormatToString(handle->drm_format);
        return -EINVAL;
    }
  } else {  // num_planes == 3
    out_ycbcr->chroma_step = 1;
    switch (handle->drm_format) {
      case DRM_FORMAT_YUV420:
        out_ycbcr->cb = addr[1] + handle->offsets[1];
        out_ycbcr->cr = addr[2] + handle->offsets[2];
        break;

      case DRM_FORMAT_YVU420:
        out_ycbcr->cb = addr[2] + handle->offsets[2];
        out_ycbcr->cr = addr[1] + handle->offsets[1];
        break;

      default:
        LOGF(ERROR) << "Unsupported semi-planar format: "
                    << FormatToString(handle->drm_format);
        return -EINVAL;
    }
  }
  return 0;
}

int CameraBufferMapper::Unlock(buffer_handle_t buffer) {
  for (size_t i = 0; i < GetNumPlanes(buffer); ++i) {
    int ret = Unmap(buffer, i);
    if (ret) {
      return ret;
    }
  }
  return 0;
}

void* CameraBufferMapper::Map(buffer_handle_t buffer,
                              uint32_t flags,
                              uint32_t x,
                              uint32_t y,
                              uint32_t width,
                              uint32_t height,
                              uint32_t plane) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return MAP_FAILED;
  }

  uint32_t num_planes = GetNumPlanes(buffer);
  if (!num_planes) {
    return MAP_FAILED;
  }
  if (!(plane < kMaxPlanes && plane < num_planes &&
        (x + width) <= handle->width && (y + height) <= handle->height)) {
    LOGF(ERROR) << "Invalid args: x=" << x << " y=" << y << " width=" << width
                << " height=" << height << " plane=" << plane;
    return MAP_FAILED;
  }

  VLOGF(1) << "buffer info:";
  VLOGF(1) << "\tfd: " << handle->fds[plane].get();
  VLOGF(1) << "\tbuffer_id: 0x" << std::hex << handle->buffer_id;
  VLOGF(1) << "\ttype: " << handle->type;
  VLOGF(1) << "\tformat: " << FormatToString(handle->drm_format);
  VLOGF(1) << "\twidth: " << handle->width;
  VLOGF(1) << "\theight: " << handle->height;
  VLOGF(1) << "\tstride: " << handle->strides[plane];
  VLOGF(1) << "\toffset: " << handle->offsets[plane];

  base::AutoLock l(lock_);

  if (handle->type == GRALLOC) {
    struct MappedGrallocBufferInfo* info;
    auto key = MappedGrallocBufferInfoCache::key_type(buffer, plane);
    auto info_cache = buffer_info_.find(key);
    if (info_cache == buffer_info_.end()) {
      // We haven't mapped |plane| of |buffer| yet.
      info = new struct MappedGrallocBufferInfo;
      auto it = buffer_context_.find(buffer);
      if (it == buffer_context_.end()) {
        LOGF(ERROR) << "Buffer 0x" << std::hex << handle->buffer_id
                    << " is not registered";
        return MAP_FAILED;
      }
      info->bo = it->second->bo;
    } else {
      // We have mapped |plane| on |buffer| before: we can simply call
      // gbm_bo_map() on the existing bo.
      DCHECK(buffer_context_.find(buffer) != buffer_context_.end());
      info = info_cache->second.get();
    }
    uint32_t stride;
    void* out_addr = gbm_bo_map(info->bo, x, y, width, height, flags, &stride,
                                &info->map_data, plane);
    if (out_addr == MAP_FAILED) {
      LOGF(ERROR) << "Failed to map buffer: " << strerror(errno);
      return MAP_FAILED;
    }
    // Only increase the usage count and insert |info| into the cache after the
    // buffer is successfully mapped.
    info->usage++;
    if (info_cache == buffer_info_.end()) {
      buffer_info_[key].reset(info);
    }
    VLOGF(1) << "Plane " << plane << " of gralloc buffer 0x" << std::hex
             << handle->buffer_id << " mapped";
    return out_addr;
  } else if (handle->type == SHM) {
    // We can't call mmap() here because each mmap call may return different
    // mapped virtual addresses and may lead to virtual memory address leak.
    // Instead we call mmap() only once in Register.
    auto it = buffer_context_.find(buffer);
    if (it == buffer_context_.end()) {
      LOGF(ERROR) << "Unknown buffer 0x" << std::hex << handle->buffer_id;
      return MAP_FAILED;
    }
    auto buffer_context = it->second.get();
    void* out_addr = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(buffer_context->mapped_addr) +
        handle->offsets[plane] + y * handle->strides[plane] + x);
    VLOGF(1) << "Plane " << plane << " of shm buffer 0x" << std::hex
             << handle->buffer_id << " mapped";
    return out_addr;
  } else {
    NOTREACHED() << "Invalid buffer type: " << handle->type;
    return MAP_FAILED;
  }
}

int CameraBufferMapper::Unmap(buffer_handle_t buffer, uint32_t plane) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return -EINVAL;
  }

  if (handle->type == GRALLOC) {
    base::AutoLock l(lock_);
    auto key = MappedGrallocBufferInfoCache::key_type(buffer, plane);
    auto info_cache = buffer_info_.find(key);
    if (info_cache == buffer_info_.end()) {
      LOGF(ERROR) << "Plane " << plane << " of buffer 0x" << std::hex
                  << handle->buffer_id << " was not mapped";
      return -EINVAL;
    }
    auto& info = info_cache->second;
    if (info->usage) {
      // We rely on GBM's internal refcounting mechanism to unmap the bo when
      // appropriate.
      gbm_bo_unmap(info->bo, info->map_data);
      if (!--info->usage) {
        buffer_info_.erase(info_cache);
      }
    }
  } else if (handle->type == SHM) {
    // No-op for SHM buffers.
  } else {
    NOTREACHED() << "Invalid buffer type: " << handle->type;
    return -EINVAL;
  }
  VLOGF(1) << "buffer 0x" << std::hex << handle->buffer_id << " unmapped";
  return 0;
}

// static
uint32_t CameraBufferMapper::GetNumPlanes(buffer_handle_t buffer) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return 0;
  }

  switch (handle->drm_format) {
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_AYUV:
    case DRM_FORMAT_BGR233:
    case DRM_FORMAT_BGR565:
    case DRM_FORMAT_BGR888:
    case DRM_FORMAT_BGRA1010102:
    case DRM_FORMAT_BGRA4444:
    case DRM_FORMAT_BGRA5551:
    case DRM_FORMAT_BGRA8888:
    case DRM_FORMAT_BGRX1010102:
    case DRM_FORMAT_BGRX4444:
    case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_BGRX8888:
    case DRM_FORMAT_C8:
    case DRM_FORMAT_GR88:
    case DRM_FORMAT_R8:
    case DRM_FORMAT_RG88:
    case DRM_FORMAT_RGB332:
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_RGBA8888:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_RGBX4444:
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_XBGR4444:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
      return 1;
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_NV21:
      return 2;
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_YVU420:
      return 3;
  }

  LOGF(ERROR) << "Unknown format: " << FormatToString(handle->drm_format);
  return 0;
}

// static
uint32_t CameraBufferMapper::GetV4L2PixelFormat(buffer_handle_t buffer) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return 0;
  }

  switch (handle->drm_format) {
    case DRM_FORMAT_ARGB8888:
      return V4L2_PIX_FMT_ABGR32;

    // There is no standard V4L2 pixel format corresponding to
    // DRM_FORMAT_xBGR8888.  We use our own V4L2 format extension
    // V4L2_PIX_FMT_RGBX32 here.
    case DRM_FORMAT_ABGR8888:
      return V4L2_PIX_FMT_RGBX32;
    case DRM_FORMAT_XBGR8888:
      return V4L2_PIX_FMT_RGBX32;

    // DRM_FORMAT_R8 is used as the underlying buffer format for
    // HAL_PIXEL_FORMAT_BLOB which corresponds to JPEG buffer.
    case DRM_FORMAT_R8:
      return V4L2_PIX_FMT_JPEG;

    // Semi-planar formats.
    case DRM_FORMAT_NV12:
      return V4L2_PIX_FMT_NV21M;
    case DRM_FORMAT_NV21:
      return V4L2_PIX_FMT_NV12M;

    // Multi-planar formats.
    case DRM_FORMAT_YUV420:
      return V4L2_PIX_FMT_YUV420M;
    case DRM_FORMAT_YVU420:
      return V4L2_PIX_FMT_YVU420M;
  }

  LOGF(ERROR) << "Could not convert format "
              << FormatToString(handle->drm_format) << " to V4L2 pixel format";
  return 0;
}

}  // namespace arc
