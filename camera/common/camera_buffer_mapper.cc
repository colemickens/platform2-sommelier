/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "arc/camera_buffer_mapper.h"

#include <sys/mman.h>

#include <drm_fourcc.h>
#include <gbm.h>

#include "arc/common.h"
#include "common/camera_buffer_handle.h"
#include "common/camera_buffer_mapper_internal.h"

namespace arc {

// static
CameraBufferMapper* CameraBufferMapper::GetInstance() {
  static CameraBufferMapper instance;
  if (!instance.gbm_device_) {
    LOG(ERROR) << "Failed to create GBM device for CameraBufferMapper";
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
    auto bo_cache = gbm_bo_.find(buffer);
    if (bo_cache == gbm_bo_.end()) {
      GbmBoInfoUniquePtr bo_info(new struct GbmBoInfo);
      // Import the buffer if we haven't done so.
      struct gbm_import_fd_planar_data import_data;
      memset(&import_data, 0, sizeof(import_data));
      import_data.width = handle->width;
      import_data.height = handle->height;
      import_data.format = handle->format;
      int num_planes = GetNumPlanes(buffer);
      if (num_planes <= 0) {
        return -EINVAL;
      }
      for (size_t i = 0; i < num_planes; ++i) {
        import_data.fds[i] = handle->fds[i];
        import_data.strides[i] = handle->strides[i];
        import_data.offsets[i] = handle->offsets[i];
      }

      bo_info->bo = gbm_bo_import(gbm_device_.get(), GBM_BO_IMPORT_FD_PLANAR,
                                  &import_data, GBM_BO_USE_RENDERING);
      if (!bo_info->bo) {
        LOG(ERROR) << "Failed to import buffer 0x" << std::hex
                   << handle->buffer_id;
        return -EIO;
      }
      bo_info->usage = 1;
      gbm_bo_[buffer] = std::move(bo_info);
    } else {
      bo_cache->second->usage++;
    }
    return 0;
  } else if (handle->type == SHM) {
    // TODO(jcliang): Implement register for shared memory buffers.
    LOG(ERROR)
        << "Register for shared memory buffer handle is not yet implemented";
    return -EINVAL;
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

  if (handle->type == GRALLOC) {
    auto bo_cache = gbm_bo_.find(buffer);
    if (bo_cache == gbm_bo_.end()) {
      LOG(ERROR) << "Unknown buffer 0x" << std::hex << handle->buffer_id;
      return -EINVAL;
    }
    if (!--bo_cache->second->usage) {
      // Unmap all the existing mapping of bo.
      for (auto it = buffer_info_.begin(); it != buffer_info_.end();) {
        if (it->second->bo == bo_cache->second->bo) {
          it = buffer_info_.erase(it);
        } else {
          ++it;
        }
      }
      gbm_bo_.erase(bo_cache);
    }
    return 0;
  } else if (handle->type == SHM) {
    // TODO(jcliang): Implement unregister for shared memory buffers.
    LOG(ERROR)
        << "Unegister for shared memory buffer handle is not yet implemented";
    return -EINVAL;
  } else {
    NOTREACHED() << "Invalid buffer type: " << handle->type;
    return -EINVAL;
  }
}

void* CameraBufferMapper::Map(buffer_handle_t buffer,
                              uint32_t flags,
                              uint32_t x,
                              uint32_t y,
                              uint32_t width,
                              uint32_t height,
                              uint32_t plane,
                              uint32_t* out_stride) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return MAP_FAILED;
  }

  int num_planes = GetNumPlanes(buffer);
  if (num_planes <= 0) {
    return MAP_FAILED;
  }
  if (!(plane < kMaxPlanes && plane < num_planes &&
        (x + width) <= handle->width && (y + height) <= handle->height)) {
    LOG(ERROR) << "Invalid args: x=" << x << " y=" << y << " width=" << width
               << " height=" << height << " plane=" << plane;
    return MAP_FAILED;
  }

  VLOGF(1) << "buffer info:";
  VLOGF(1) << "\tfd: " << handle->fds[plane];
  VLOGF(1) << "\tbuffer_id: 0x" << std::hex << handle->buffer_id;
  VLOGF(1) << "\ttype: " << handle->type;
  VLOGF(1) << "\tformat: " << FormatToString(handle->format);
  VLOGF(1) << "\twidth: " << handle->width;
  VLOGF(1) << "\theight: " << handle->height;
  VLOGF(1) << "\tstride: " << handle->strides[plane];
  VLOGF(1) << "\toffset: " << handle->offsets[plane];

  base::AutoLock l(lock_);

  auto key = MappedBufferInfoCache::key_type(buffer, plane);

  if (handle->type == GRALLOC) {
    struct MappedBufferInfo* info;
    auto info_cache = buffer_info_.find(key);
    if (info_cache == buffer_info_.end()) {
      // We haven't mapped |plane| of |buffer| yet.
      info = new struct MappedBufferInfo;
      info->type = static_cast<enum BufferType>(handle->type);
      auto bo_cache = gbm_bo_.find(buffer);
      if (bo_cache == gbm_bo_.end()) {
        LOG(ERROR) << "Buffer 0x" << std::hex << handle->buffer_id
                   << " is not registered";
        return MAP_FAILED;
      }
      info->bo = bo_cache->second->bo;
    } else {
      // We have mapped |plane| on |buffer| before: we can simply call
      // gbm_bo_map() on the existing bo.
      DCHECK(gbm_bo_.find(buffer) != gbm_bo_.end());
      info = info_cache->second.get();
    }
    void* out_addr = gbm_bo_map(info->bo, x, y, width, height, flags,
                                out_stride, &info->map_data, plane);
    if (out_addr == MAP_FAILED) {
      LOG(ERROR) << "Failed to map buffer: " << strerror(errno);
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
    // TODO(jcliang): Implement map for shared memory buffers.
    LOG(ERROR) << "Map for shared memory buffer handle is not yet implemented";
    return MAP_FAILED;
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

  base::AutoLock l(lock_);

  auto key = MappedBufferInfoCache::key_type(buffer, plane);
  auto info_cache = buffer_info_.find(key);
  if (info_cache == buffer_info_.end()) {
    LOG(ERROR) << "Plane " << plane << " of buffer 0x" << std::hex
               << handle->buffer_id << " was not mapped";
    return -EINVAL;
  }

  auto& info = info_cache->second;
  if (info->type == GRALLOC) {
    if (info->usage) {
      // We rely on GBM's internal refcounting mechanism to unmap the bo when
      // appropriate.
      gbm_bo_unmap(info->bo, info->map_data);
      if (!--info->usage) {
        buffer_info_.erase(info_cache);
      }
    }
  } else if (info->type == SHM) {
    // TODO(jcliang): Implement unmap for shared memory buffers.
    buffer_info_.erase(info_cache);
    LOG(ERROR)
        << "Unmap for shared memory buffer handle is not yet implemented";
    return -EINVAL;
  }
  VLOGF(1) << "buffer 0x" << std::hex << handle->buffer_id << " unmapped";
  return 0;
}

// static
int CameraBufferMapper::GetNumPlanes(buffer_handle_t buffer) {
  auto handle = camera_buffer_handle_t::FromBufferHandle(buffer);
  if (!handle) {
    return -EINVAL;
  }

  switch (handle->format) {
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
      return 2;
    case DRM_FORMAT_YVU420:
      return 3;
  }

  LOG(ERROR) << "Unknown format: " << FormatToString(handle->format);
  return -EINVAL;
}

}  // namespace arc
