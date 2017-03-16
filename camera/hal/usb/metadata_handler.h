/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_METADATA_HANDLER_H_
#define HAL_USB_METADATA_HANDLER_H_

#include <memory>

#include "arc/camera_metadata.h"
#include "hal/usb/common_types.h"

#define UPDATE(tag, data, size)                      \
  {                                                  \
    if (Update((tag), (data), (size))) {             \
      LOGF(ERROR) << "Update " << #tag << " failed"; \
      return -EINVAL;                                \
    }                                                \
  }

namespace arc {

struct CameraMetadataDeleter {
  inline void operator()(camera_metadata_t* metadata) const {
    free_camera_metadata(metadata);
  }
};

typedef std::unique_ptr<camera_metadata_t, CameraMetadataDeleter>
    CameraMetadataUniquePtr;

class MetadataHandler : public CameraMetadata {
 public:
  // Creates an empty object.
  MetadataHandler();
  // Takes ownership of passed-in buffer.
  explicit MetadataHandler(camera_metadata_t* buffer);
  // Clones the metadata.
  MetadataHandler(const MetadataHandler& other);

  // Assignment clones metadata buffer.
  MetadataHandler& operator=(const MetadataHandler& other);
  MetadataHandler& operator=(const camera_metadata_t* buffer);

  ~MetadataHandler();

  int FillDefaultMetadata();

  int FillMetadataFromSupportedFormats(
      const SupportedFormats& supported_formats);

  int FillMetadataFromDeviceInfo(const DeviceInfo& device_info);

  // Return a copy of metadata. Caller takes the ownership.
  CameraMetadataUniquePtr CreateDefaultRequestSettings(int template_type);

  static bool IsValidTemplateType(int type);
};

}  // namespace arc

#endif  // HAL_USB_METADATA_HANDLER_H_
