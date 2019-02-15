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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_MF_PARAM9_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_MF_PARAM9_H_
#pragma once

#include "../dbg_exif_def.h"

//
// Debug Exif Version 9 - MT6771
//
namespace dbg_cam_mf_param_9 {

// MF debug info
enum { MF_DEBUG_TAG_VERSION = 9 };
enum { MF_DEBUG_TAG_SUBVERSION = 4 };
const unsigned int MF_DEBUG_TAG_VERSION_DP =
    ((MF_DEBUG_TAG_SUBVERSION << 16) | MF_DEBUG_TAG_VERSION);

// MF Parameter Structure
typedef enum {
  // ------------------------------------------------------------------------
  // MFNR SW related info
  // {{{
  // BEGIN_OF_EXIF_TAG
  MF_TAG_VERSION =
      0,  // MF tag version. Bits[0:15]: major ver, Bits[16:31]: sub version
  // ------------------------------------------------------------------------
  // Extension: HDR related tags
  // {{{
  MF_TAG_IMAGE_HDR,
  // }}}

  // indicates to size
  MF_DEBUG_TAG_SIZE,
  // END_OF_EXIF_TAG
} DEBUG_MF_TAG_T;

typedef struct DEBUG_MF_INFO_S {
  debug_exif_field Tag[MF_DEBUG_TAG_SIZE];
} DEBUG_MF_INFO_T;

};      // namespace dbg_cam_mf_param_9
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_MF_PARAM9_H_
