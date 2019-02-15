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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_EIS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_EIS_H_

#include <camera_custom_eis_base.h>

class VISIBILITY_PUBLIC EISCustom : public EISCustomBase {
 private:
  // DO NOT create instance
  EISCustom() {}

 public:
  using EISCustomBase::USAGE_MASK;
  using EISCustomBase::VIDEO_CFG;

  // EIS state
  static MUINT32 getEISMode(MUINT32 mask);

  // EIS customized data
  static void getEISData(EIS_Customize_Para_t* a_pDataOut);
  static void getEISPlusData(EIS_PLUS_Customize_Para_t* a_pDataOut,
                             MUINT32 config);
  static void getEIS25Data(EIS25_Customize_Tuning_Para_t* a_pDataOut);
  static void getEIS30Data(EIS30_Customize_Tuning_Para_t* a_pDataOut);

  // EIS version support
  static MBOOL isForcedEIS12();
  static MBOOL isEnabledEIS22();
  static MBOOL isEnabledEIS25();
  static MBOOL isEnabledEIS30();
  static MBOOL isEnabledFixedFPS();
  static MBOOL isEnabledForwardMode(MUINT32 cfg = 0);
  static MBOOL isEnabledGyroMode();
  static MBOOL isEnabledImageMode();
  static MBOOL isEnabledLosslessMode();
  static MBOOL isEnabledFOVWarpCombine(MUINT32 cfg = 0);
  static MBOOL isEnabledLMVData();

  // EIS configurations
  static double getEISRatio(MUINT32 cfg = 0);
  static MUINT32 getEIS12Factor();
  static MUINT32 getEISFactor(MUINT32 cfg = 0);
  static MUINT32 get4K2KRecordFPS();
  static MUINT32 getForwardStartFrame();
  static MUINT32 getForwardFrames(MUINT32 cfg = 0);
  static void getMVNumber(MINT32 width,
                          MINT32 height,
                          MINT32* mvWidth,
                          MINT32* mvHeight);

 private:
  static void appendEISMode(MUINT32 mask, MUINT32* eisMode);
  static MBOOL generateEIS22Mode(MUINT32 mask, MUINT32* eisMode);
  static MBOOL generateEIS25FusionMode(MUINT32 mask, MUINT32* eisMode);
  static MBOOL generateEIS25ImageMode(MUINT32 mask, MUINT32* eisMode);
  static MBOOL generateEIS25GyroMode(MUINT32 mask, MUINT32* eisMode);
  static MBOOL generateEIS30FusionMode(MUINT32 mask, MUINT32* eisMode);
  static MBOOL generateEIS30ImageMode(MUINT32 mask, MUINT32* eisMode);
  static MBOOL generateEIS30GyroMode(MUINT32 mask, MUINT32* eisMode);
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_EIS_H_
