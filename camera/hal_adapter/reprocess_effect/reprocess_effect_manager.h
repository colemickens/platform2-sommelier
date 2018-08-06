/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_REPROCESS_EFFECT_REPROCESS_EFFECT_MANAGER_H_
#define HAL_ADAPTER_REPROCESS_EFFECT_REPROCESS_EFFECT_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cros-camera/camera_buffer_manager.h"
#include "hal_adapter/reprocess_effect/portrait_mode_effect.h"
#include "hal_adapter/reprocess_effect/reprocess_effect.h"

namespace cros {

class ReprocessEffectManager {
 public:
  ReprocessEffectManager();

  ~ReprocessEffectManager() = default;

  int32_t Initialize();

  // Get all vendor tags.  It returns a map of tag names and types with tag as
  // the index.
  int32_t GetAllVendorTags(
      std::unordered_map<uint32_t, std::pair<std::string, uint8_t>>*
          vendor_tag_map);

  // Handle the reprocessing request.  It returns -ENOENT error if no
  // corresponding vendor tag is found in |settings|.  On success, it stores
  // result vendor tags into |result_metadata| and the caller should merge them
  // into the result metadata of capture result.
  int32_t ReprocessRequest(const camera_metadata_t& settings,
                           buffer_handle_t input_buffer,
                           uint32_t width,
                           uint32_t height,
                           android::CameraMetadata* result_metadata,
                           buffer_handle_t output_buffer);

 private:
  struct VendorTagInfo {
    VendorTagInfo(const std::string& n, uint8_t t, ReprocessEffect* e)
        : name(n), type(t), effect(e) {}
    const std::string name;
    const uint8_t type;
    // Pointing to the reprocessing effect that allocates this request vendor
    // tag, or nullptr if this is a result vendor tag.
    ReprocessEffect* effect;
  };

  // Map of vendor tag info with vendor tag as the key.  In the future, we may
  // want to move the management of vendor tags out when reprocessing effect
  // manager is not the sole user of vendor tags.
  std::unordered_map<uint32_t, VendorTagInfo> vendor_tag_info_map_;

  // Next available vendor tag
  uint32_t max_vendor_tag_;

  CameraBufferManager* buffer_manager_;

  std::unique_ptr<PortraitModeEffect> portrait_mode_;

  DISALLOW_COPY_AND_ASSIGN(ReprocessEffectManager);
};

}  // namespace cros

#endif  // HAL_ADAPTER_REPROCESS_EFFECT_REPROCESS_EFFECT_MANAGER_H_
