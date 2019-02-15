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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_METADATA_CUSTOM_METADATA_TAG_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_METADATA_CUSTOM_METADATA_TAG_H_

typedef enum custom_camera_metadata_section {
  CUSTOM_CAMERA_METADATA_SECTION = 0xC000,  // WARNING!!!! do not modify 0xC000

  // Add your metadata section from here
  CUSTOM_DEVICE_CAPABILITIES,
  CUSTOM_CAPTURE_METADATA,
  CUSTOM_CAMERA_METADATA_SECTION_COUNT,
} custom_camera_metadata_section_t;

typedef enum custom_camera_metadata_section_start {
  CUSTOM_DEVICE_CAPABILITIES_START = CUSTOM_DEVICE_CAPABILITIES << 16,
  CUSTOM_CAPTURE_METADATA_START = CUSTOM_CAPTURE_METADATA << 16,
} custom_camera_metadata_section_start_t;

/**
 * Main enum for defining camera metadata tags.  New entries must always go
 * before the section _END tag to preserve existing enumeration values.  In
 * addition, the name and type of the tag needs to be added to
 * ""
 */
typedef enum custom_camera_metadata_tag {
  ANDROID_HW_SENSOR_WB_RANGE = CUSTOM_DEVICE_CAPABILITIES_START,
  CUSTOM_DEVICE_CAPABILITIES_END,

  ANDROID_HW_SENSOR_WB_VALUE = CUSTOM_CAPTURE_METADATA_START,
  CUSTOM_CAPTURE_METADATA_END,
} custom_camera_metadata_tag_t;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_METADATA_CUSTOM_METADATA_TAG_H_
