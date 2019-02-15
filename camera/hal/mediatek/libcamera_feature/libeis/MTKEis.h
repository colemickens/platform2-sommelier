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

#ifndef CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBEIS_MTKEIS_H_
#define CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBEIS_MTKEIS_H_

#include "MTKEisType.h"
#include "MTKEisErrCode.h"
#include <mtkcam/utils/module/module.h>

#define EIS_WIN_NUM 32

typedef enum {
  EIS_STATE_STANDBY,
  EIS_STATE_INIT,
  EIS_STATE_PROC,
  EIS_STATE_FINISH,
  EIS_STATE_IDLE
} EIS_STATE_ENUM;

typedef enum { EIS_PATH_RAW_DOMAIN, EIS_PATH_YUV_DOMAIN } EIS_INPUT_PATH_ENUM;

typedef enum {
  EIS_SENSI_LEVEL_HIGH = 0,
  EIS_SENSI_LEVEL_NORMAL = 1,
  EIS_SENSI_LEVEL_ADVTUNE = 2
} EIS_SENSITIVITY_ENUM;

typedef enum { ABSOLUTE_HIST_METHOD, SMOOTH_HIST_METHOD } EIS_VOTE_METHOD_ENUM;

// IMPORTANTAT! - Do not modify the adv. tuning parameters at will
typedef struct {
  MUINT32 new_tru_th;         // 0~100
  MUINT32 vot_th;             // 1~16
  MUINT32 votb_enlarge_size;  // 0~1280
  MUINT32 min_s_th;           // 10~100
  MUINT32 vec_th;             // 0~11   should be even
  MUINT32 spr_offset;         // 0 ~ MarginX/2
  MUINT32 spr_gain1;          // 0~127
  MUINT32 spr_gain2;          // 0~127
  MUINT32 gmv_pan_array[4];   // 0~5
  MUINT32 gmv_sm_array[4];    // 0~5
  MUINT32 cmv_pan_array[4];   // 0~5
  MUINT32 cmv_sm_array[4];    // 0~5

  EIS_VOTE_METHOD_ENUM vot_his_method;  // 0 or 1
  MUINT32 smooth_his_step;              // 2~6
  MUINT32 eis_debug;
} EIS_ADV_TUNING_PARA_STRUCT, *P_EIS_ADV_TUNING_PARA_STRUCT;

typedef struct {
  EIS_SENSITIVITY_ENUM sensitivity;  // 0 or 1 or 2
  MUINT32 filter_small_motion;       // 0 or 1
  MUINT32 adv_shake_ext;             // 0 or 1
  MFLOAT stabilization_strength;     // 0.5~0.95
  EIS_ADV_TUNING_PARA_STRUCT advtuning_data;
} EIS_TUNING_PARA_STRUCT, *P_EIS_TUNING_PARA_STRUCT;

typedef struct {
  EIS_TUNING_PARA_STRUCT eis_tuning_data;
  EIS_INPUT_PATH_ENUM Eis_Input_Path;
} EIS_SET_ENV_INFO_STRUCT, *P_EIS_SET_ENV_INFO_STRUCT;

typedef struct {
  MINT32 CMV_X;
  MINT32 CMV_Y;
} EIS_RESULT_INFO_STRUCT, *P_EIS_RESULT_INFO_STRUCT;

typedef struct {
  MINT32 i4LMV_X[EIS_WIN_NUM];
  MINT32 i4LMV_Y[EIS_WIN_NUM];

  MINT32 i4LMV_X2[EIS_WIN_NUM];
  MINT32 i4LMV_Y2[EIS_WIN_NUM];

  MUINT32 NewTrust_X[EIS_WIN_NUM];
  MUINT32 NewTrust_Y[EIS_WIN_NUM];

  MUINT32 SAD[EIS_WIN_NUM];
  MUINT32 SAD2[EIS_WIN_NUM];
  MUINT32 AVG_SAD[EIS_WIN_NUM];
} EIS_STATISTIC_STRUCT;

typedef struct {
  MINT32 EIS_GMVx;
  MINT32 EIS_GMVy;
} EIS_GMV_INFO_STRUCT, *P_EIS_GMV_INFO_STRUCT;

typedef struct {
  MUINT32 InputWidth;
  MUINT32 InputHeight;
  MUINT32 TargetWidth;
  MUINT32 TargetHeight;
} EIS_CONFIG_IMAGE_INFO_STRUCT, *P_EIS_CONFIG_IMAGE_INFO_STRUCT;

typedef struct {
  MFLOAT GMVx;
  MFLOAT GMVy;
  MINT32 ConfX;
  MINT32 ConfY;
} EIS_GET_PLUS_INFO_STRUCT, *P_EIS_GET_PLUS_INFO_STRUCT;

typedef struct {
  MBOOL GyroValid;
  MBOOL Gvalid;
  MFLOAT GyroInfo[3];
  MFLOAT AcceInfo[3];
} EIS_SENSOR_INFO_STRUCT, *P_EIS_SENSOR_INFO_STRUCT;

typedef struct {
  EIS_STATISTIC_STRUCT eis_state;
  EIS_CONFIG_IMAGE_INFO_STRUCT eis_image_size_config;
  EIS_SENSOR_INFO_STRUCT sensor_info;
  MINT32 DivH;
  MINT32 DivV;
  MUINT32 EisWinNum;
} EIS_SET_PROC_INFO_STRUCT, *P_EIS_SET_PROC_INFO_STRUCT;

typedef enum {
  EIS_FEATURE_BEGIN = 0,
  EIS_FEATURE_SET_PROC_INFO,
  EIS_FEATURE_GET_PROC_INFO,
  EIS_FEATURE_GET_DEBUG_INFO,
  EIS_FEATURE_SET_DEBUG_INFO,
  EIS_FEATURE_GET_EIS_STATE,
  EIS_FEATURE_SAVE_LOG,
  EIS_FEATURE_GET_ORI_GMV,
  EIS_FEATURE_GET_EIS_PLUS_DATA,
  EIS_FEATURE_MAX
} EIS_FEATURE_ENUM;

typedef struct {
  MUINT32 FrameNum;
  EIS_STATISTIC_STRUCT sEIS_Ori_Stat;  // 900 bytes
  MINT32 u4GMVx;
  MINT32 u4GMVy;
  MINT32 u4CMVx;
  MINT32 u4CMVy;
  MINT32 u4SmoothGMVx;
  MINT32 u4SmoothGMVy;
  MINT32 u4SmoothCMVx;
  MINT32 u4SmoothCMVy;
  MUINT32 u4WeightX[EIS_WIN_NUM];
  MUINT32 u4WeightY[EIS_WIN_NUM];
  MUINT16 u4VoteIndeX;
  MUINT16 u4VoteIndeY;
  MFLOAT FinalCoefBld[6];
} EIS_DEBUG_TAG_STRUCT, *P_EIS_DEBUG_TAG_STRUCT;

typedef struct {
  void* Eis_Log_Buf_Addr;
  MUINT32 Eis_Log_Buf_Size;
} EIS_SET_LOG_BUFFER_STRUCT, *P_EIS_SET_LOG_BUFFER_STRUCT;

class MTKEis {
 public:
  static MTKEis* createInstance();
  virtual void destroyInstance() = 0;

  virtual ~MTKEis() {}
  // Process Control
  virtual NSCam::MRESULT EisInit(void* InitInData);
  virtual NSCam::MRESULT EisMain(EIS_RESULT_INFO_STRUCT* EisResult);  // START
  virtual NSCam::MRESULT EisReset();                                  // Reset

  // Feature Control
  virtual NSCam::MRESULT EisFeatureCtrl(MUINT32 FeatureID,
                                        void* pParaIn,
                                        void* pParaOut);

 private:
};

typedef MTKEis* (*NR3D_FACTORY_T)(MUINT32 const openId);
#define MAKE_3DNR_IPC(...) \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_3DNR_IPC, NR3D_FACTORY_T, __VA_ARGS__)

#endif  // CAMERA_HAL_MEDIATEK_LIBCAMERA_FEATURE_LIBEIS_MTKEIS_H_
