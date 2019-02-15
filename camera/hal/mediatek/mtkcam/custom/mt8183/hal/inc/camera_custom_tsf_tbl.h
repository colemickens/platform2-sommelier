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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_TSF_TBL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_TSF_TBL_H_

typedef struct {
  int isTsfEn;       // isEnableTSF
  int tsfCtIdx;      // getTSFD65Idx
  int rAWBInput[8];  // getTSFAWBForceInput: lv, cct, fluo, day fluo, rgain,
                     // bgain, ggain, rsvd
} CMAERA_TSF_CFG_STRUCT, *PCMAERA_TSF_CFG_STRUCT;

typedef struct {
  int TSF_para[1620];
} CAMERA_TSF_PARA_STRUCT, *PCAMERA_TSF_PARA_STRUCT;

typedef struct {
  unsigned int tsf_data[16000];
} CAMERA_TSF_DATA_STRUCT, *PCAMERA_TSF_DATA_STRUCT;

typedef struct {
  CMAERA_TSF_CFG_STRUCT TSF_CFG;
  int TSF_PARA[1620];
  unsigned int TSF_DATA[16000];
} CAMERA_TSF_TBL_STRUCT, *PCAMERA_TSF_TBL_STRUCT;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_TSF_TBL_H_
