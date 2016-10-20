/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef USB_CAMERA_METADATA_H_
#define USB_CAMERA_METADATA_H_

#include <memory>

#include "arc/metadata_base.h"
#include "usb/common_types.h"

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

class CameraMetadata : public MetadataBase {
 public:
  // Creates an empty object.
  CameraMetadata();
  // Takes ownership of passed-in buffer.
  explicit CameraMetadata(camera_metadata_t* buffer);
  // Clones the metadata.
  CameraMetadata(const CameraMetadata& other);

  // Assignment clones metadata buffer.
  CameraMetadata& operator=(const CameraMetadata& other);
  CameraMetadata& operator=(const camera_metadata_t* buffer);

  ~CameraMetadata();

  int FillDefaultMetadata();

  int FillMetadataFromSupportedFormats(
      const SupportedFormats& supported_formats);

  int FillMetadataFromDeviceInfo(const DeviceInfo& device_info);

  // Return a copy of metadata. Caller takes the ownership.
  CameraMetadataUniquePtr CreateDefaultRequestSettings(int template_type);

  static bool IsValidTemplateType(int type);
};

}  // namespace arc

#endif  // USB_CAMERA_METADATA_H_
