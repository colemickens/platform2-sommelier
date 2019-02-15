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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_ISP_TUNING_ISP_TUNING_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_ISP_TUNING_ISP_TUNING_H_

#include <isp_tuning/isp_tuning_sensor.h>
#include "../tuning_mapping/cam_idx_struct_ext.h"

namespace NSIspTuning {

/*******************************************************************************
 *
 ******************************************************************************/
typedef enum MERROR_ENUM {
  MERR_OK = 0,
  MERR_UNKNOWN = 0x80000000,  // Unknown error
  MERR_UNSUPPORT,
  MERR_BAD_PARAM,
  MERR_BAD_CTRL_CODE,
  MERR_BAD_FORMAT,
  MERR_BAD_ISP_DRV,
  MERR_BAD_NVRAM_DRV,
  MERR_BAD_SENSOR_DRV,
  MERR_BAD_SYSRAM_DRV,
  MERR_SET_ISP_REG,
  MERR_NO_MEM,
  MERR_NO_SYSRAM_MEM,
  MERR_NO_RESOURCE,
  MERR_CUSTOM_DEFAULT_INDEX_NOT_FOUND,
  MERR_CUSTOM_NOT_READY,
  MERR_PREPARE_HW,
  MERR_APPLY_TO_HW,
  MERR_CUSTOM_ISO_ENV_ERR,
  MERR_CUSTOM_CT_ENV_ERR
} MERROR_ENUM_T;

/*******************************************************************************
 * Operation Mode
 ******************************************************************************/
typedef enum {
  EOperMode_Normal = 0,
  EOperMode_PureRaw,
  EOperMode_Meta,
  EOperMode_EM,
  EOperMpde_Factory
} EOperMode_T;

/*******************************************************************************
 * PCA Mode
 ******************************************************************************/
typedef enum { EPCAMode_180BIN = 0, EPCAMode_360BIN, EPCAMode_NUM } EPCAMode_T;

typedef enum {
  ESensorTG_None = 0,
  ESensorTG_1,
  ESensorTG_2,
} ESensorTG_T;

typedef enum { ERawType_Proc = 0, ERawType_Pure = 1 } ERawType_T;

};  // namespace NSIspTuning

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_ISP_TUNING_ISP_TUNING_H_
