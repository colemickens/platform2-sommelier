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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_DBG_ID_PARAM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_DBG_ID_PARAM_H_
#pragma once

/******************************************************************************
 *
 ******************************************************************************/

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Debug Exif Key ID
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define DEBUG_EXIF_KEYID_AAA 0xF0F1F200
#define DEBUG_EXIF_KEYID_ISP 0xF4F5F6F7
#define DEBUG_EXIF_KEYID_CAM 0xF8F9FAFB
#define DEBUG_EXIF_KEYID_SHAD_TABLE 0xFCFDFEFF
#define DEBUG_EXIF_KEYID_EIS 0xF1F3F5F7

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Debug Exif Module ID - DEBUG_EXIF_KEYID_CAM
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
enum {
  DEBUG_EXIF_MID_CAM_CMN = 0x5001,
  DEBUG_EXIF_MID_CAM_MF,
  DEBUG_EXIF_MID_CAM_N3D,
  DEBUG_EXIF_MID_CAM_SENSOR,
  DEBUG_EXIF_MID_CAM_RESERVE1,
  DEBUG_EXIF_MID_CAM_RESERVE2,
  DEBUG_EXIF_MID_CAM_RESERVE3,
};

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Debug Exif Module ID - DEBUG_EXIF_KEYID_EIS
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
enum {
  DEBUG_EXIF_MID_EIS = 0x4001,
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_DBG_ID_PARAM_H_
