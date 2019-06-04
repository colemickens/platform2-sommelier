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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCSWNR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCSWNR_H_

#include "IPCCommon.h"
#include "mtkcam/aaa/ICaptureNR.h"
#include "mtkcam/def/common.h"

namespace IPCSWNR {

#define MAX_SWNR_HAL_META_SIZE 8192

struct CommonParams {
  MINT32 sensor_idx;
};

struct CreateParams {
  CommonParams common;
};

struct DestroyParams {
  CommonParams common;
};

struct DoSwNrParams {
  CommonParams common;
  ISwNR::SWNRParam swnr_param;
  IImagebufInfo imagebuf_info;
};

struct GetDebugInfoParams {
  CommonParams common;
  char hal_metadata[MAX_SWNR_HAL_META_SIZE];
};

}  // namespace IPCSWNR

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCSWNR_H_
