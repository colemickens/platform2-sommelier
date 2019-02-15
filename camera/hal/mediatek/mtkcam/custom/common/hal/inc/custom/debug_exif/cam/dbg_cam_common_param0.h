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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_COMMON_PARAM0_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_COMMON_PARAM0_H_
#pragma once

/******************************************************************************
 *
 ******************************************************************************/
#include "../dbg_exif_def.h"

namespace dbg_cam_common_param_0 {
/******************************************************************************
 *
 ******************************************************************************/

// Common debug info
enum { CMN_DEBUG_TAG_VERSION = 0 };
enum { CMN_DEBUG_TAG_SIZE = 10 };

// Common Parameter Structure
typedef enum {
  CMN_TAG_VERSION = 0,
  CMN_TAG_SHOT_MODE,
  CMN_TAG_CAM_MODE
} DEBUG_CMN_TAG_T;

typedef struct DEBUG_CMN_INFO_S {
  debug_exif_field Tag[CMN_DEBUG_TAG_SIZE];
} DEBUG_CMN_INFO_T;

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace dbg_cam_common_param_0

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_COMMON_PARAM0_H_
