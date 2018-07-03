// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CAMERA3_TEST_CAMERA3_EXIF_VALIDATOR_H_
#define CAMERA3_TEST_CAMERA3_EXIF_VALIDATOR_H_

#include <base/logging.h>
#include <exif-data.h>

#include "camera3_test/camera3_device_fixture.h"

namespace camera3_test {

class Camera3ExifValidator {
 public:
  struct JpegExifInfo {
    const BufferHandleUniquePtr& buffer_handle;
    size_t buffer_size;
    void* buffer_addr;
    ResolutionInfo jpeg_resolution;
    ExifData* exif_data;
    JpegExifInfo(const BufferHandleUniquePtr& buffer, size_t size);
    ~JpegExifInfo();
    bool Initialize();
  };

  struct ExifTestData {
    ResolutionInfo thumbnail_resolution;
    int32_t orientation;
    uint8_t jpeg_quality;
    uint8_t thumbnail_quality;
  };

  explicit Camera3ExifValidator(const Camera3Device::StaticInfo& cam_info)
      : cam_info_(cam_info) {}

  void ValidateExifKeys(const ResolutionInfo& jpeg_resolution,
                        const ExifTestData& exif_test_data,
                        const BufferHandleUniquePtr& buffer,
                        size_t buffer_size,
                        const camera_metadata_t& metadata,
                        const time_t& date_time) const;

 protected:
  const Camera3Device::StaticInfo& cam_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Camera3ExifValidator);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_EXIF_VALIDATOR_H_
