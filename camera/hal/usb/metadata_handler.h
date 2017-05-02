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

// MetadataHandler is thread-safe. It is used for saving metadata states of
// CameraDevice.
class MetadataHandler {
 public:
  explicit MetadataHandler(const camera_metadata_t& metadata);
  ~MetadataHandler();

  static int FillDefaultMetadata(CameraMetadata* metadata);

  static int FillMetadataFromSupportedFormats(
      const SupportedFormats& supported_formats,
      CameraMetadata* metadata);

  static int FillMetadataFromDeviceInfo(const DeviceInfo& device_info,
                                        CameraMetadata* metadata);

  // Get default settings according to the |template_type|. Can be called on
  // any thread.
  const camera_metadata_t* GetDefaultRequestSettings(int template_type);

  // Called after the request is processed. This function is used to update
  // required metadata which can be gotten from 3A or image processor.
  void PostHandleRequest(int64_t timestamp, CameraMetadata* metadata);

 private:
  // Check |template_type| is valid or not.
  bool IsValidTemplateType(int template_type);

  // Return a copy of metadata according to |template_type|.
  CameraMetadataUniquePtr CreateDefaultRequestSettings(int template_type);

  // Metadata containing persistent camera characteristics.
  CameraMetadata metadata_;

  // Static array of standard camera settings templates. These are owned by
  // CameraClient.
  CameraMetadataUniquePtr template_settings_[CAMERA3_TEMPLATE_COUNT];
};

}  // namespace arc

#endif  // HAL_USB_METADATA_HANDLER_H_
