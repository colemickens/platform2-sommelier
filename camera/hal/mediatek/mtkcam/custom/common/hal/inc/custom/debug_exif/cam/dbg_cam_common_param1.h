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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_COMMON_PARAM1_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_COMMON_PARAM1_H_
#pragma once

/******************************************************************************
 *
 ******************************************************************************/
#include "../dbg_exif_def.h"

namespace dbg_cam_common_param_1 {
/******************************************************************************
 *
 ******************************************************************************/

// Common debug info
enum { CMN_DEBUG_TAG_VERSION = 1 };
enum { CMN_DEBUG_TAG_SUBVERSION = 1 };
const unsigned int CMN_DEBUG_TAG_VERSION_DP =
    ((CMN_DEBUG_TAG_SUBVERSION << 16) | CMN_DEBUG_TAG_VERSION);

// Common Parameter Structure
typedef enum {
  // BEGIN_OF_EXIF_TAG
  CMN_TAG_VERSION = 0,
  CMN_TAG_SHOT_MODE,
  CMN_TAG_CAM_MODE,
  CMN_TAG_PIPELINE_UNIQUE_KEY,
  CMN_TAG_PIPELINE_FRAME_NUMBER,
  CMN_TAG_PIPELINE_REQUEST_NUMBER,
  CMN_TAG_DOWNSCALE_DENOISE_THRES,
  CMN_TAG_DOWNSCALE_DENOISE_RATIO,
  CMN_TAG_DOWNSCALE_DENOISE_WIDTH,
  CMN_TAG_DOWNSCALE_DENOISE_HIGHT,
  CMN_TAG_SWNR_THRESHOLD,
  CMN_DEBUG_TAG_SIZE  // END_OF_EXIF_TAG
} DEBUG_CMN_TAG_T;

typedef struct DEBUG_CMN_INFO_S {
  debug_exif_field Tag[CMN_DEBUG_TAG_SIZE];
} DEBUG_CMN_INFO_T;

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace dbg_cam_common_param_1

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_COMMON_PARAM1_H_
