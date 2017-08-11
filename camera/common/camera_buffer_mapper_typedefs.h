/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef COMMON_CAMERA_BUFFER_MAPPER_TYPEDEFS_H_
#define COMMON_CAMERA_BUFFER_MAPPER_TYPEDEFS_H_

#include <unistd.h>

#include <memory>
#include <unordered_map>
#include <utility>

#include <gbm.h>

#include <base/macros.h>

#define EXPORTED __attribute__((__visibility__("default")))

struct native_handle;
typedef const native_handle* buffer_handle_t;
struct android_ycbcr;

namespace arc {

// The enum definition here should match |Camera3DeviceOps::BufferType| in
// hal_adapter/arc_camera3.mojom.
enum BufferType {
  GRALLOC = 0,
  SHM = 1,
};

// This class wraps gbm_device for sharing it with tests
class EXPORTED GbmDevice {
 public:
  int IsFormatSupported(uint32_t format, uint32_t usage);

  struct gbm_bo* CreateBo(uint32_t width,
                          uint32_t height,
                          uint32_t format,
                          uint32_t flags);

  operator bool() const { return device_ != nullptr; }

 private:
  friend class CameraBufferMapperImpl;

  GbmDevice();

  ~GbmDevice();

  struct Deleter {
    void operator()(GbmDevice* device);
  };

  struct gbm_device* device_;

  DISALLOW_COPY_AND_ASSIGN(GbmDevice);
};

struct BufferContext {
  // ** The following fields are used for gralloc buffers only. **
  // The GBM bo of the gralloc buffer.
  struct gbm_bo* bo;
  // ** End of gralloc buffer fields. **

  // ** The following fields are used for shm buffers only. **
  // The mapped address of the shared memory buffer.
  void* mapped_addr;

  // The size of the shared memory buffer.
  size_t shm_buffer_size;
  // ** End of shm buffer fields. **

  uint32_t usage;

  BufferContext()
      : bo(nullptr), mapped_addr(nullptr), shm_buffer_size(0), usage(0) {}

  ~BufferContext() {
    if (bo) {
      gbm_bo_destroy(bo);
    }
  }
};

typedef std::unordered_map<buffer_handle_t,
                           std::unique_ptr<struct BufferContext>>
    BufferContextCache;

struct MappedGrallocBufferInfo {
  // The gbm_bo associated with the imported buffer (for gralloc buffer only).
  struct gbm_bo* bo;
  // The per-bo data returned by gbm_bo_map() (for gralloc buffer only).
  void* map_data;
  // The mapped virtual address.
  void* addr;
  // For refcounting.
  uint32_t usage;

  MappedGrallocBufferInfo() : bo(nullptr), map_data(nullptr), usage(0) {}

  ~MappedGrallocBufferInfo() {
    if (bo && map_data) {
      gbm_bo_unmap(bo, map_data);
    }
  }
};

typedef std::pair<buffer_handle_t, uint32_t> MappedBufferInfoKeyType;

struct MappedBufferInfoKeyHash {
  size_t operator()(const MappedBufferInfoKeyType& key) const {
    // The key is (buffer_handle_t pointer, plane number).  Plane number is less
    // than 4, so shifting the pointer value left by 8 and filling the lowest
    // byte with the plane number gives us a unique value to represent a key.
    return (reinterpret_cast<size_t>(key.first) << 8 | key.second);
  }
};

typedef std::unordered_map<MappedBufferInfoKeyType,
                           std::unique_ptr<MappedGrallocBufferInfo>,
                           struct MappedBufferInfoKeyHash>
    MappedGrallocBufferInfoCache;

}  // namespace arc

#endif  // COMMON_CAMERA_BUFFER_MAPPER_TYPEDEFS_H_
