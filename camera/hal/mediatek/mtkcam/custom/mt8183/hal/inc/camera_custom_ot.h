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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_OT_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_OT_H_

typedef struct {
  int OBLoseTrackingFrm;
  int OCLoseTrackingFrm;
  float LtOcOb_ColorSimilarity_TH;
  float ARFA;
  int Numiter_shape_F;
  int LightResistance;
  int MaxObjHalfSize;
  int MinObjHalfSize;
  int IniwinW;
  int IniwinH;
  int AEAWB_LOCK;
} OT_Customize_PARA;

void get_ot_CustomizeData(OT_Customize_PARA* OTDataOut);

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_OT_H_
