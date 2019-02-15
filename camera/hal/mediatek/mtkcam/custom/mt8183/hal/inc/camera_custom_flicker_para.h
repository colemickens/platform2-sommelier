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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_FLICKER_PARA_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_FLICKER_PARA_H_

enum {
  e_sensorModePreview = 0,
  e_sensorModeVideoPreview,
  e_sensorModeVideo = e_sensorModeVideoPreview,
  e_sensorModeCapture,
  e_sensorModeZsd = e_sensorModeCapture,
  e_sensorModeVideo1,
  e_sensorModeVideo2,
  e_sensorModeCustom1,
  e_sensorModeCustom2,
  e_sensorModeCustom3,
  e_sensorModeCustom4,
  e_sensorModeCustom5,
};

typedef struct {
  MINT32 m;
  MINT32 b_l;
  MINT32 b_r;
  MINT32 offset;
} FLICKER_CUST_STATISTICS;

typedef struct {
  MINT32 flickerFreq[9];
  MINT32 flickerGradThreshold;
  MINT32 flickerSearchRange;
  MINT32 minPastFrames;
  MINT32 maxPastFrames;
  FLICKER_CUST_STATISTICS EV50_L50;
  FLICKER_CUST_STATISTICS EV50_L60;
  FLICKER_CUST_STATISTICS EV60_L50;
  FLICKER_CUST_STATISTICS EV60_L60;
  MINT32 EV50_thresholds[2];
  MINT32 EV60_thresholds[2];
  MINT32 freq_feature_index[2];
} FLICKER_CUST_PARA;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_FLICKER_PARA_H_
