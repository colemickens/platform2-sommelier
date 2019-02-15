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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_MF_PARAM5_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_MF_PARAM5_H_
#pragma once

#include "../dbg_exif_def.h"

//
// Debug Exif Version 5 - MT6757
//
namespace dbg_cam_mf_param_5 {

// MF debug info
enum { MF_DEBUG_TAG_VERSION = 5 };  // MT6757
enum { MF_DEBUG_TAG_SUBVERSION = 0 };

// MF Parameter Structure
typedef enum {
  // ------------------------------------------------------------------------
  // MFNR SW related info
  // {{{
  MF_TAG_VERSION = 0,  // sw info. n, for 3.n
  /* indicates to size */
  MF_DEBUG_TAG_SIZE,
} DEBUG_MF_TAG_T;

typedef struct DEBUG_MF_INFO_S {
  debug_exif_field Tag[MF_DEBUG_TAG_SIZE];
} DEBUG_MF_INFO_T;

};      // namespace dbg_cam_mf_param_5
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_MF_PARAM5_H_
