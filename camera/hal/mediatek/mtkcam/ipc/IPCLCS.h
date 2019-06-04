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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCLCS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCLCS_H_

#include "mtkcam/aaa/lcs/lcs_hal.h"
#include "mtkcam/def/common.h"

namespace IPCLCS {

struct CommonParams {
  MINT32 sensor_idx;
  MINT32 buffer_handler;
};

struct CreateParams {
  CommonParams common;
};

struct InitParams {
  CommonParams common;
};

struct UninitParams {
  CommonParams common;
};

struct ConfigParams {
  CommonParams common;
  LCS_HAL_CONFIG_DATA config_data;
};

}  // namespace IPCLCS

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCLCS_H_
