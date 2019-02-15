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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_N3D_SYNC2A_TUNING_PARAM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_N3D_SYNC2A_TUNING_PARAM_H_

#define MAX_MAPPING_DELTABV_ISPRATIO 30
typedef enum {
  SYNC_DUAL_CAM_DENOISE_BMDN = 0,
  SYNC_DUAL_CAM_DENOISE_MFNR,
  SYNC_DUAL_CAM_DENOISE_MAX
} SYNC_DUAL_CAM_DENOISE_ENUM;
typedef struct {
  MBOOL isFinerSyncOffset;     // False: 10base, TRUE: 1000base
  MINT32 EVOffset_main[2];     // EVOffset main
  MINT32 EVOffset_main2[2];    // EVOffset main2
  MUINT32 RGB2YCoef_main[3];   // RGB2YCoef main
  MUINT32 RGB2YCoef_main2[3];  // RGB2YCoef main2
  MUINT32 FixSyncGain;
  MUINT32 u4RegressionType;  // 0:cwv 1:avg
  MBOOL isDoGainRegression;
  MUINT16 SyncWhichEye;
  MUINT32 pDeltaBVtoRatioArray[SYNC_DUAL_CAM_DENOISE_MAX]
                              [MAX_MAPPING_DELTABV_ISPRATIO];
} strSyncAEInitInfo;

typedef struct {
  MBOOL PP_method_consider_ccm;
  MUINT32 PP_method_Y_threshold_low;
  MUINT32 PP_method_Y_threshold_high;
  MUINT32 PP_method_valid_block_num_ratio;  // %precentage

  MUINT32 PP_method_valid_childblock_percent_th;
  MUINT32 PP_method_diff_percent_th;
  MUINT32 PP_method_enhance_change;
} SYNC_AWB_ADV_PP_METHOD_TUNING_STRUCT;

typedef struct {
  MUINT32 BlendingTh[4];
} SYNC_AWB_BLEND_METHOD_TUNING_STRUCT;

typedef struct {
  MUINT32 WP_Gr_ratio_th_main[2];  // [0] low bound, [1] high bound
  MUINT32 WP_Gb_ratio_th_main[2];

  MUINT32 WP_Gr_ratio_th_sub[2];
  MUINT32 WP_Gb_ratio_th_sub[2];

  MUINT8 WP_area_hit_ratio;
  MUINT8 WP_PP_gain_ratio_tbl[2];

  MUINT32 WP_Y_th[2];
} SYNC_AWB_WP_METHOD_TUNING_STRUCT;

typedef struct {
  MBOOL bEnSyncWBSmooth;
  MUINT32 u4SyncAWBSmoothParam[3];
} SYNC_AWBGAIN_SMOOTH_TUNING_STRUCT;

typedef struct {
  MINT32 PP_method_Tungsten_prefer[3];
  MINT32 PP_method_WarmFL_prefer[3];
  MINT32 PP_method_FL_prefer[3];
  MINT32 PP_method_CWF_prefer[3];
  MINT32 PP_method_Day_prefer[3];
  MINT32 PP_method_Shade_prefer[3];
  MINT32 PP_method_DayFL_prefer[3];
} SYNC_AWBGAIN_PREFER_TUNING_STRUCT;

typedef struct {
  MINT32 PP_method_prefer_LV_TH[2];
  MINT32 PP_method_Tungsten_prefer_LV_R[2];
  MINT32 PP_method_Tungsten_prefer_LV_G[2];
  MINT32 PP_method_Tungsten_prefer_LV_B[2];
  MINT32 PP_method_WarmFL_prefer_LV_R[2];
  MINT32 PP_method_WarmFL_prefer_LV_G[2];
  MINT32 PP_method_WarmFL_prefer_LV_B[2];
  MINT32 PP_method_FL_prefer_LV_R[2];
  MINT32 PP_method_FL_prefer_LV_G[2];
  MINT32 PP_method_FL_prefer_LV_B[2];
  MINT32 PP_method_CWF_prefer_LV_R[2];
  MINT32 PP_method_CWF_prefer_LV_G[2];
  MINT32 PP_method_CWF_prefer_LV_B[2];
  MINT32 PP_method_Day_prefer_LV_R[2];
  MINT32 PP_method_Day_prefer_LV_G[2];
  MINT32 PP_method_Day_prefer_LV_B[2];
  MINT32 PP_method_Shade_prefer_LV_R[2];
  MINT32 PP_method_Shade_prefer_LV_G[2];
  MINT32 PP_method_Shade_prefer_LV_B[2];
  MINT32 PP_method_DayFL_prefer_LV_R[2];
  MINT32 PP_method_DayFL_prefer_LV_G[2];
  MINT32 PP_method_DayFL_prefer_LV_B[2];
} SYNC_AWBGAIN_PREFER_LVBASE_STRUCT;

typedef struct {
  MUINT32 syncAWBMethod;  // Choose use which kind of sync method

  SYNC_AWB_ADV_PP_METHOD_TUNING_STRUCT syncAWB_pp_method_tuning_param;
  SYNC_AWB_BLEND_METHOD_TUNING_STRUCT syncAWB_blend_method_tuning_param;
  SYNC_AWB_WP_METHOD_TUNING_STRUCT syncAWB_wp_method_tuning_param;

  MBOOL isFixMapLocation;
  MUINT32 u4SyncAWB_fov_ratio;

  SYNC_AWBGAIN_SMOOTH_TUNING_STRUCT syncAWB_smooth_param;
  SYNC_AWBGAIN_PREFER_TUNING_STRUCT syncAWB_prefer_param;
  SYNC_AWBGAIN_PREFER_LVBASE_STRUCT syncAWB_prefer_lvbase;
} strSyncAWBInitInfo;

const strSyncAEInitInfo* getSyncAEInitInfo();
const strSyncAWBInitInfo* getSyncAWBInitInfo();

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_N3D_SYNC2A_TUNING_PARAM_H_
