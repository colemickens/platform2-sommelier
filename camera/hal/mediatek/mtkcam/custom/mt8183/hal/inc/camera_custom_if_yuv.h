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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_IF_YUV_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_IF_YUV_H_
//
#include "inc/camera_custom_types.h"

//
namespace NSCamCustom {

typedef struct {
  double dFlashlightThreshold;
  MINT32 i4FlashlightDuty;
  MINT32 i4FlashlightStep;
  MINT32 i4FlashlightFrameCnt;
  MINT32 i4FlashlightPreflashAF;
  MINT32 i4FlashlightGain10X;
  MINT32 i4FlashlightHighCurrentDuty;
  MINT32 i4FlashlightHighCurrentTimeout;
  MINT32 i4FlashlightAfLampSupport;
} YUV_FL_PARAM_T;

void custom_GetYuvFLParam(MINT32 i4ParId, YUV_FL_PARAM_T* rYuvFlParam);

};      // namespace NSCamCustom
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_IF_YUV_H_
