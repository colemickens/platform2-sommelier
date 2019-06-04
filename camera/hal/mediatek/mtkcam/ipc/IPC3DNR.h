/*
 * Copyright (C) 2019 Mediatek Corporation.
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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPC3DNR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPC3DNR_H_
#include "mtkcam/def/common.h"
#include <libeis/MTKEis.h>

namespace IPC3DNR {

struct nr3d_common_params {
  MINT32 sensor_idx;
  MINT32 buffer_handler;
};

struct nr3d_featurectrl_params {
  nr3d_common_params nr3d_common;
  int eFeatureCtrl;
  union {
    EIS_SET_PROC_INFO_STRUCT ipcEisProcInfo;
    EIS_GET_PLUS_INFO_STRUCT ipcEisPlusData;
    EIS_GMV_INFO_STRUCT ipcEisOriGmv;
  } arg;
};

struct nr3d_init_params {
  nr3d_common_params nr3d_common;
  EIS_SET_ENV_INFO_STRUCT ipcEisInitData;
};

struct nr3d_create_params {
  nr3d_common_params nr3d_common;
};

struct nr3d_destory_params {
  nr3d_common_params nr3d_common;
};

struct nr3d_reset_params {
  nr3d_common_params nr3d_common;
};

struct nr3d_main_params {
  nr3d_common_params nr3d_common;
  EIS_RESULT_INFO_STRUCT ipcEisMianData;
};
}  // namespace IPC3DNR

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPC3DNR_H_
