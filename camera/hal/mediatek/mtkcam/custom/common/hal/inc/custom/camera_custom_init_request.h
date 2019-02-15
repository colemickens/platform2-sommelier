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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_CAMERA_CUSTOM_INIT_REQUEST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_CAMERA_CUSTOM_INIT_REQUEST_H_

/**************************************************************************
 *                      D E F I N E S / M A C R O S                       *
 **************************************************************************/
// For Init Request Switch on/off for boost startPreview

//     CUST_START_PREVIEW_INIT_REQUEST_ENABLE is a ON/OFF switch for
//     enable/disable startPreview boost
//     - define 1 : to enable startPreview boost
//     - define 0 : to disable startPreview boost
#define CUST_START_PREVIEW_INIT_REQUEST_ENABLE 0

//     CUST_START_PREVIEW_INIT_REQUEST_NUM is const value for startPreview boost
//     - The value is decided by middleware & 3A, can't be modified byself
#define CUST_START_PREVIEW_INIT_REQUEST_NUM 4

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_CAMERA_CUSTOM_INIT_REQUEST_H_
