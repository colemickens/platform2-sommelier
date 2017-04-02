/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_COMMON_TYPES_H_
#define HAL_ADAPTER_COMMON_TYPES_H_

#include <map>
#include <memory>

#include "common/camera_buffer_handle.h"
#include "hardware/camera3.h"

namespace internal {

// Common data types for hal_adapter internal use.

struct CameraMetadataDeleter {
  inline void operator()(camera_metadata_t* metadata) const {
    free_camera_metadata(metadata);
  }
};

typedef std::unique_ptr<camera_metadata_t, CameraMetadataDeleter>
    CameraMetadataUniquePtr;

typedef std::map<uint64_t, std::unique_ptr<camera3_stream_t>> UniqueStreams;

}  // namespace internal

#endif  // HAL_ADAPTER_COMMON_TYPES_H_
