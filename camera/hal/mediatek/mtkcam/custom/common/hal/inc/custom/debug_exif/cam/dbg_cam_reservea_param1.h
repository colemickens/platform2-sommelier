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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_RESERVEA_PARAM1_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_RESERVEA_PARAM1_H_
#pragma once

/******************************************************************************
 *
 ******************************************************************************/
#include "../dbg_exif_def.h"

namespace dbg_cam_reservea_param_1 {
/******************************************************************************
 *
 ******************************************************************************/

// TEST_A debug info
enum { RESERVEA_DEBUG_TAG_VERSION = 1 };
enum { DBG_NR_SIZE = 98 };  // max(MNR, SWNR, fSWNR)
enum { RESERVEA_DEBUG_TAG_SIZE = (1 /*version*/ + DBG_NR_SIZE /*NRs*/) };

// ReserveA Parameter Structure
typedef enum {
  RESERVEA_TAG_VERSION = 0,
  /* add tags here */
  RESERVEA_TAG_END
} DEBUG_RESERVEA_TAG_T;

struct DEBUG_RESERVEA_INFO_T {
  debug_exif_field Tag[RESERVEA_DEBUG_TAG_SIZE];
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace dbg_cam_reservea_param_1

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_RESERVEA_PARAM1_H_
