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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_CAPTURE_NR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_CAPTURE_NR_H_
#include <mtkcam/def/common.h>

#define DISABLE_CAPTURE_NR (999999)

enum ESWNRType {
  eSWNRType_SW,
  eSWNRType_SW2,
  eSWNRType_SW2_VPU,
  eSWNRType_NUM,
};

enum SWNR_PerfLevel {
  eSWNRPerf_Performance_First,
  eSWNRPerf_Custom0,
  eSWNRPerf_Power_First,
  eSWNRPerf_Default = eSWNRPerf_Performance_First,
};

bool VISIBILITY_PUBLIC get_capture_nr_th(MUINT32 const sensorDev,
                                         MUINT32 const shotmode,
                                         MBOOL const isMfll,
                                         int* hwth,
                                         int* swth);

MINT32 get_performance_level(MUINT32 const sensorDev,
                             MUINT32 const shotmode,
                             MBOOL const isMfll,
                             MBOOL const isMultiOpen);

MBOOL is_to_invoke_swnr_interpolation(MUINT32 scenario, MUINT32 u4Iso);
MINT32 get_swnr_type(MUINT32 const sensorDev);

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_CAPTURE_NR_H_
