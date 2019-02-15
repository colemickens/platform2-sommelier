/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_METADATA_CUSTOM_VENDORTAG_TABLE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_METADATA_CUSTOM_VENDORTAG_TABLE_H_

#include "custom_metadata/custom_metadata_tag.h"
#include <map>
#include <vector>
#define CUSTOM_NAME "com.custom"

static auto& _custom_device_capability_() {
  static const std::map<uint32_t, VendorTag_t> sInst = {
      _TAG_(ANDROID_HW_SENSOR_WB_RANGE, "sensorWbRange", TYPE_INT32),
  };
  return sInst;
}

static auto& _custom_capture_metadata_() {
  static const std::map<uint32_t, VendorTag_t> sInst = {
      _TAG_(ANDROID_HW_SENSOR_WB_VALUE, "sensorWbValue", TYPE_INT32),
  };
  //
  return sInst;
}

/******************************************************************************
 *
 ******************************************************************************/
static auto& getCustomGlobalSections() {
  static const std::vector<VendorTagSection_t> sCustomSections = {
      _SECTION_(CUSTOM_NAME ".device.capabilities",
                CUSTOM_DEVICE_CAPABILITIES_START,
                CUSTOM_DEVICE_CAPABILITIES_END, _custom_device_capability_()),
      _SECTION_(CUSTOM_NAME ".capture.metadata", CUSTOM_CAPTURE_METADATA_START,
                CUSTOM_CAPTURE_METADATA_END, _custom_capture_metadata_()),
  };
  return sCustomSections;
}

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_METADATA_CUSTOM_VENDORTAG_TABLE_H_
