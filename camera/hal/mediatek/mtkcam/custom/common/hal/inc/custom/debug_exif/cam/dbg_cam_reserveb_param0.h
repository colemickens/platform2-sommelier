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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_RESERVEB_PARAM0_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_RESERVEB_PARAM0_H_
#pragma once

/******************************************************************************
 *
 ******************************************************************************/
#include "../dbg_exif_def.h"

namespace dbg_cam_reserveb_param_0 {
/******************************************************************************
 *
 ******************************************************************************/

// ReserveB Parameter Structure
typedef enum {
  // BEGIN_OF_EXIF_TAG
  RESERVEB_TAG_VERSION = 0,
  RESERVEB_TAG_END
  // END_OF_EXIF_TAG
} DEBUG_RESERVEB_TAG_T;

// TEST_B debug info
enum { RESERVEB_DEBUG_TAG_VERSION = 0 };
enum { RESERVEB_DEBUG_TAG_SUBVERSION = 0 };
const unsigned int RESERVEB_DEBUG_TAG_VERSION_DP =
    ((RESERVEB_DEBUG_TAG_SUBVERSION << 16) | RESERVEB_DEBUG_TAG_VERSION);
enum { RESERVEB_DEBUG_TAG_SIZE = (RESERVEB_TAG_END + 10) };

struct DEBUG_RESERVEB_INFO_T {
  debug_exif_field Tag[RESERVEB_DEBUG_TAG_SIZE];
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace dbg_cam_reserveb_param_0

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_RESERVEB_PARAM0_H_
