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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_N3D_PARAM1_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_N3D_PARAM1_H_
#pragma once

/******************************************************************************
 *
 ******************************************************************************/
#include "../dbg_exif_def.h"

namespace dbg_cam_n3d_param_1 {
/******************************************************************************
 *
 ******************************************************************************/
enum { N3D_AE_TAG_DEBUG_VERSION = 0, N3D_AE_TAG_MAX };

// N3D AE debug info
enum { N3D_AE_DEBUG_VERSION = 2 };
enum { N3D_AE_DEBUG_TAG_SIZE = (N3D_AE_TAG_MAX + 10) };

typedef struct {
  debug_exif_field Tag[N3D_AE_DEBUG_TAG_SIZE];
} N3D_AE_DEBUG_INFO_T;

/******************************************************************************
 *
 ******************************************************************************/
enum {
  // Intermedium Data
  N3D_AWB_TAG_DEBUG_VERSION = N3D_AE_TAG_MAX,
  N3D_AWB_TAG_MAX
};

// N3D AWB debug info
enum { N3D_AWB_DEBUG_VERSION = 2 };
enum { N3D_AWB_DEBUG_TAG_SIZE = (N3D_AWB_TAG_MAX + 10) };

typedef struct {
  debug_exif_field Tag[N3D_AWB_DEBUG_TAG_SIZE];
} N3D_AWB_DEBUG_INFO_T;

/******************************************************************************
 *
 ******************************************************************************/
// Common Parameter Structure
typedef enum {
  N3D_TAG_VERSION = 0,
} DEBUG_N3D_TAG_T;

// Native3D debug info
enum { N3D_DEBUG_TAG_SIZE = (N3D_AE_DEBUG_TAG_SIZE + N3D_AWB_DEBUG_TAG_SIZE) };
enum { N3D_DEBUG_TAG_VERSION = 1 };
enum { DEBUG_N3D_AE_MODULE_ID = 0x0001 };
enum { DEBUG_N3D_AWB_MODULE_ID = 0x0002 };

typedef struct DEBUG_N3D_INFO_S {
  debug_exif_field Tag[N3D_DEBUG_TAG_SIZE];
} DEBUG_N3D_INFO_T;

typedef struct {
  N3D_AE_DEBUG_INFO_T rAEDebugInfo;
  N3D_AWB_DEBUG_INFO_T rAWBDebugInfo;
} N3D_DEBUG_INFO_T;

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace dbg_cam_n3d_param_1

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_N3D_PARAM1_H_
