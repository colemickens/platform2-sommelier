/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hal_adapter/reprocess_effect/reprocess_effect_manager.h"

#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/logging.h>
#include <hardware/gralloc.h>

#include "cros-camera/common.h"
#include "hal_adapter/reprocess_effect/portrait_mode_effect.h"
#include "hal_adapter/vendor_tags.h"

namespace cros {

ReprocessEffectManager::ReprocessEffectManager()
    : max_vendor_tag_(VENDOR_GOOGLE_START),
      buffer_manager_(CameraBufferManager::GetInstance()) {}

int32_t ReprocessEffectManager::Initialize() {
  VLOGF_ENTER();
  portrait_mode_ = std::make_unique<PortraitModeEffect>();
  std::vector<std::pair<std::string, uint8_t>> request_vendor_tags;
  std::vector<std::pair<std::string, uint8_t>> result_vendor_tags;
  if (portrait_mode_->InitializeAndGetVendorTags(&request_vendor_tags,
                                                 &result_vendor_tags) != 0) {
    LOGF(ERROR) << "Failed to initialize portrait mode effect";
    return -ENODEV;
  }
  if (!request_vendor_tags.empty() || !result_vendor_tags.empty()) {
    uint32_t request_vendor_tag_start = max_vendor_tag_;
    for (const auto& it : request_vendor_tags) {
      vendor_tag_info_map_.emplace(
          max_vendor_tag_,
          VendorTagInfo(it.first, it.second, portrait_mode_.get()));
      max_vendor_tag_++;
    }
    uint32_t result_vendor_tag_start = max_vendor_tag_;
    for (const auto& it : result_vendor_tags) {
      vendor_tag_info_map_.emplace(max_vendor_tag_,
                                   VendorTagInfo(it.first, it.second, nullptr));
      max_vendor_tag_++;
    }
    if (portrait_mode_->SetVendorTags(
            request_vendor_tag_start,
            result_vendor_tag_start - request_vendor_tag_start,
            result_vendor_tag_start,
            max_vendor_tag_ - result_vendor_tag_start) != 0) {
      LOGF(ERROR) << "Failed to set portrait mode effect vendor tags";
      return -ENODEV;
    }
  }
  return 0;
}

int32_t ReprocessEffectManager::GetAllVendorTags(
    std::unordered_map<uint32_t, std::pair<std::string, uint8_t>>*
        vendor_tag_map) {
  if (!vendor_tag_map) {
    return -EINVAL;
  }
  for (const auto& it : vendor_tag_info_map_) {
    vendor_tag_map->emplace(it.first,
                            std::make_pair(it.second.name, it.second.type));
  }
  return 0;
}

int32_t ReprocessEffectManager::ReprocessRequest(
    const camera_metadata_t& settings,
    buffer_handle_t input_buffer,
    uint32_t width,
    uint32_t height,
    android::CameraMetadata* result_metadata,
    buffer_handle_t output_buffer) {
  VLOGF_ENTER();
  // TODO(hywu): enable cascading effects
  for (uint32_t tag = VENDOR_GOOGLE_START; tag < max_vendor_tag_; tag++) {
    camera_metadata_ro_entry_t entry;
    if (find_camera_metadata_ro_entry(&settings, tag, &entry) == 0) {
      DCHECK(vendor_tag_info_map_.find(tag) != vendor_tag_info_map_.end());
      if (!vendor_tag_info_map_.at(tag).effect) {
        LOGF(WARNING) << "Received result vendor tag 0x" << std::hex << tag
                      << " in request";
        continue;
      }
      int result = 0;
      uint32_t v4l2_format = buffer_manager_->GetV4L2PixelFormat(output_buffer);
      result = vendor_tag_info_map_.at(tag).effect->ReprocessRequest(
          settings, input_buffer, width, height, v4l2_format, result_metadata,
          output_buffer);
      if (result != 0) {
        LOGF(ERROR) << "Failed to handle reprocess request on vendor tag 0x"
                    << std::hex << tag;
      }
      return result;
    }
  }
  return -ENOENT;
}

}  // namespace cros
