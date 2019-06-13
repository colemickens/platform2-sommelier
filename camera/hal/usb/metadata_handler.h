/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_HAL_USB_METADATA_HANDLER_H_
#define CAMERA_HAL_USB_METADATA_HANDLER_H_

#include <memory>

#include <base/threading/thread_checker.h>
#include <camera/camera_metadata.h>
#include <hardware/camera3.h>

#include "hal/usb/common_types.h"
#include "hal/usb/sensor_handler.h"

namespace cros {

struct CameraMetadataDeleter {
  inline void operator()(camera_metadata_t* metadata) const {
    free_camera_metadata(metadata);
  }
};

typedef std::unique_ptr<camera_metadata_t, CameraMetadataDeleter>
    ScopedCameraMetadata;

// MetadataHandler is thread-safe. It is used for saving metadata states of
// CameraDevice.
class MetadataHandler {
 public:
  MetadataHandler(const camera_metadata_t& metadata,
                  const DeviceInfo& device_info,
                  const SupportedFormats& supported_formats);
  ~MetadataHandler();

  static int FillDefaultMetadata(android::CameraMetadata* metadata);

  static int FillMetadataFromSupportedFormats(
      const SupportedFormats& supported_formats,
      android::CameraMetadata* metadata,
      bool is_external);

  static int FillMetadataFromDeviceInfo(const DeviceInfo& device_info,
                                        android::CameraMetadata* metadata);

  // Get default settings according to the |template_type|. Can be called on
  // any thread.
  const camera_metadata_t* GetDefaultRequestSettings(int template_type);

  // PreHandleRequest and PostHandleRequest should run on the same thread.

  // Called before the request is processed. This function is used for checking
  // metadata values to setup related states and image settings.
  void PreHandleRequest(int frame_number,
                        const android::CameraMetadata& metadata);

  // Called after the request is processed. This function is used to update
  // required metadata which can be gotton from 3A or image processor.
  int PostHandleRequest(int frame_number,
                        int64_t timestamp,
                        android::CameraMetadata* metadata,
                        const Size& resolution);

 private:
  // Check |template_type| is valid or not.
  bool IsValidTemplateType(int template_type);

  // Return a copy of metadata according to |template_type|.
  ScopedCameraMetadata CreateDefaultRequestSettings(int template_type);
  int FillDefaultPreviewSettings(android::CameraMetadata* metadata);
  int FillDefaultStillCaptureSettings(android::CameraMetadata* metadata);
  int FillDefaultVideoRecordSettings(android::CameraMetadata* metadata);
  int FillDefaultVideoSnapshotSettings(android::CameraMetadata* metadata);
  int FillDefaultZeroShutterLagSettings(android::CameraMetadata* metadata);
  int FillDefaultManualSettings(android::CameraMetadata* metadata);

  // Metadata containing persistent camera characteristics.
  android::CameraMetadata metadata_;

  // Static array of standard camera settings templates. These are owned by
  // CameraClient.
  ScopedCameraMetadata template_settings_[CAMERA3_TEMPLATE_COUNT];

  // Use to check PreHandleRequest and PostHandleRequest are called on the same
  // thread.
  base::ThreadChecker thread_checker_;

  // Camera device information.
  const DeviceInfo device_info_;

  int current_frame_number_;

  bool af_trigger_;

  // Sensor handler to get sensor related metadata.
  std::unique_ptr<SensorHandler> sensor_handler_;
};

}  // namespace cros

#endif  // CAMERA_HAL_USB_METADATA_HANDLER_H_
