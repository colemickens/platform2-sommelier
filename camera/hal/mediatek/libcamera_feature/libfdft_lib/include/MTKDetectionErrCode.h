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

#ifndef CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBFDFT_LIB_INCLUDE_MTKDETECTIONERRCODE_H_
#define CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBFDFT_LIB_INCLUDE_MTKDETECTIONERRCODE_H_
#include "../include/common.h"

#define MODULE_MTK_Detection (0)  // Temp value

#define MTKDetection_OKCODE(errid) OKCODE(MODULE_MTK_Detection, errid)
#define MTKDetection_ERRCODE(errid) ERRCODE(MODULE_MTK_Detection, errid)

// Detection error code
#define S_Detection_OK MTKDetection_OKCODE(0x0000)

#define E_Detection_NEED_OVER_WRITE MTKDetection_ERRCODE(0x0001)
#define E_Detection_NULL_OBJECT MTKDetection_ERRCODE(0x0002)
#define E_Detection_WRONG_STATE MTKDetection_ERRCODE(0x0003)
#define E_Detection_WRONG_CMD_ID MTKDetection_ERRCODE(0x0004)
#define E_Detection_WRONG_CMD_PARAM MTKDetection_ERRCODE(0x0005)
#define E_Detection_Driver_Fail MTKDetection_ERRCODE(0x0010)

#define E_Detection_ERR MTKDetection_ERRCODE(0x0100)

#endif  // CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBFDFT_LIB_INCLUDE_MTKDETECTIONERRCODE_H_
