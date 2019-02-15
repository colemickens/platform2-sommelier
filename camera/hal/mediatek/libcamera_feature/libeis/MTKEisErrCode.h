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

#ifndef CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBEIS_MTKEISERRCODE_H_
#define CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBEIS_MTKEISERRCODE_H_
#include "../include/common.h"

#define MODULE_MTK_EIS (5)  // Temp value

#define MTKEIS_OKCODE(errid) OKCODE(MODULE_MTK_EIS, errid)
#define MTKEIS_ERRCODE(errid) ERRCODE(MODULE_MTK_EIS, errid)

// Error code
#define S_EIS_OK MTKEIS_OKCODE(0x0000)

#define E_EIS_NEED_OVER_WRITE MTKEIS_ERRCODE(0x0001)
#define E_EIS_NULL_OBJECT MTKEIS_ERRCODE(0x0002)
#define E_EIS_WRONG_STATE MTKEIS_ERRCODE(0x0003)
#define E_EIS_WRONG_CMD_ID MTKEIS_ERRCODE(0x0004)
#define E_EIS_WRONG_CMD_PARAM MTKEIS_ERRCODE(0x0005)
#define E_EIS_NOT_ENOUGH_MEM MTKEIS_ERRCODE(0x0006)
#define E_EIS_NULL_INIT_PARAM MTKEIS_ERRCODE(0x0007)
#define E_EIS_LOG_BUFFER_NOT_ENOUGH MTKEIS_ERRCODE(0x0008)
#define E_EIS_SET_NULL_PROCESS_INFO MTKEIS_ERRCODE(0x0009)
#define E_EIS_SET_NULL_IMAGE_SIZE_INFO MTKEIS_ERRCODE(0x000A)
#define E_EIS_SET_WRONG_IMAGE_SIZE MTKEIS_ERRCODE(0x000B)
#define E_EIS_OPEN_LOG_FILE_ERROR MTKEIS_ERRCODE(0x000C)

#define E_EIS_ERR MTKEIS_ERRCODE(0x0100)

#endif  // CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBEIS_MTKEISERRCODE_H_
