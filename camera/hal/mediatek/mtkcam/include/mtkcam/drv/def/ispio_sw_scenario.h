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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ISPIO_SW_SCENARIO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ISPIO_SW_SCENARIO_H_
/******************************************************************************
 *  Camera Definitions.
 ******************************************************************************/

/**
 * @enum ESoftwareScenario
 * @brief software scenario Enumeration.
 *
 */
enum ESoftwareScenario {
  eSoftwareScenario_Main_Normal_Stream,
  eSoftwareScenario_Main_Normal_Capture,
  eSoftwareScenario_Main_VSS_Capture,
  eSoftwareScenario_Main_ZSD_Capture,
  eSoftwareScenario_Main_Mfb_Capture,
  eSoftwareScenario_Main_Mfb_Blending,
  eSoftwareScenario_Main_Mfb_Mixing,
  eSoftwareScenario_Main_Vfb_Stream_1,
  eSoftwareScenario_Main_Vfb_Stream_2,
  eSoftwareScenario_Main_Pure_Raw_Stream,
  eSoftwareScenario_Main_CC_Raw_Stream,
  eSoftwareScenario_Main2_N3D_Stream,
  eSoftwareScenario_Sub_Normal_Stream,
  eSoftwareScenario_Sub_Normal_Capture,
  eSoftwareScenario_Sub_VSS_Capture,
  eSoftwareScenario_Sub_ZSD_Capture,
  eSoftwareScenario_Sub_Mfb_Capture,
  eSoftwareScenario_Sub_Mfb_Blending,
  eSoftwareScenario_Sub_Mfb_Mixing,
  eSoftwareScenario_Sub_Vfb_Stream_1,
  eSoftwareScenario_Sub_Vfb_Stream_2,
  eSoftwareScenario_Sub_Pure_Raw_Stream,
  eSoftwareScenario_Sub_CC_Raw_Stream,
  eSoftwareScenario_total_num
};

/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_ISPIO_SW_SCENARIO_H_
