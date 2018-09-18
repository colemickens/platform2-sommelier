/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_REPROCESS_EFFECT_PORTRAIT_MODE_EFFECT_H_
#define HAL_ADAPTER_REPROCESS_EFFECT_PORTRAIT_MODE_EFFECT_H_

#include <string>
#include <utility>
#include <vector>

#include <base/process/process.h>

#include "cros-camera/camera_buffer_manager.h"
#include "hal_adapter/reprocess_effect/reprocess_effect.h"

namespace cros {

class PortraitModeEffect final : public ReprocessEffect {
 public:
  PortraitModeEffect();

  ~PortraitModeEffect() = default;

  int32_t InitializeAndGetVendorTags(
      std::vector<VendorTagInfo>* request_vendor_tags,
      std::vector<VendorTagInfo>* result_vendor_tags);

  int32_t SetVendorTags(uint32_t request_vendor_tag_start,
                        uint32_t request_vendor_tag_count,
                        uint32_t result_vendor_tag_start,
                        uint32_t result_vendor_tag_count);

  int32_t ReprocessRequest(const camera_metadata_t& settings,
                           ScopedYUVBufferHandle* input_buffer,
                           uint32_t width,
                           uint32_t height,
                           uint32_t orientation,
                           uint32_t v4l2_format,
                           android::CameraMetadata* result_metadata,
                           ScopedYUVBufferHandle* output_buffer);

 private:
  base::Process LaunchPortraitProcessor(int input_rgb_buf_fd,
                                        int output_rgb_buf_fd,
                                        int result_report_fd,
                                        uint32_t width,
                                        uint32_t height,
                                        uint32_t orientation);

  int ConvertYUVToRGB(uint32_t v4l2_format,
                      const android_ycbcr& ycbcr,
                      void* rgb_buf_addr,
                      uint32_t rgb_buf_stride,
                      uint32_t width,
                      uint32_t height);

  int ConvertRGBToYUV(void* rgb_buf_addr,
                      uint32_t rgb_buf_stride,
                      uint32_t v4l2_format,
                      const android_ycbcr& ycbcr,
                      uint32_t width,
                      uint32_t height);

  uint32_t enable_vendor_tag_;

  enum class SegmentationResult : uint8_t { kSuccess, kFailure };

  uint32_t result_vendor_tag_;

  CameraBufferManager* buffer_manager_;

  DISALLOW_COPY_AND_ASSIGN(PortraitModeEffect);
};

}  // namespace cros

#endif  // HAL_ADAPTER_REPROCESS_EFFECT_PORTRAIT_MODE_EFFECT_H_
