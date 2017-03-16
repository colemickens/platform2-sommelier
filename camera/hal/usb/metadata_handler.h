/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_METADATA_HANDLER_H_
#define HAL_USB_METADATA_HANDLER_H_

#include <memory>

#include "arc/camera_metadata.h"
#include "hal/usb/common_types.h"

namespace arc {

struct CameraMetadataDeleter {
  inline void operator()(camera_metadata_t* metadata) const {
    free_camera_metadata(metadata);
  }
};

typedef std::unique_ptr<camera_metadata_t, CameraMetadataDeleter>
    CameraMetadataUniquePtr;

class MetadataHandler {
 public:
  static int FillDefaultMetadata(CameraMetadata* metadata);

  static int FillMetadataFromSupportedFormats(
      const SupportedFormats& supported_formats,
      CameraMetadata* metadata);

  static int FillMetadataFromDeviceInfo(const DeviceInfo& device_info,
                                        CameraMetadata* metadata);

  static bool IsValidTemplateType(int type);

  // Return a copy of metadata. Caller takes the ownership.
  static CameraMetadataUniquePtr CreateDefaultRequestSettings(
      const CameraMetadata& metadata,
      int template_type);
};

}  // namespace arc

#endif  // HAL_USB_METADATA_HANDLER_H_
