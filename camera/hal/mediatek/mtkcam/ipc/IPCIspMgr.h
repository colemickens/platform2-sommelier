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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCISPMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCISPMGR_H_

#include "mtkcam/aaa/IIspMgr.h"
#include "IPCCommon.h"

namespace NS3Av3 {

struct ispmgr_common_params {
  MINT32 sensor_idx;
};

struct ispmgr_create_params {
  ispmgr_common_params common;
};

struct ispmgr_querylcso_params {
  ispmgr_common_params common;
  LCSO_Param lcsoParam;
};

struct ispmgr_ppnr3d_params {
  ispmgr_common_params common;
  MINT32 sensorIdx;
  NR3D_Config_Param nr3dParams;
  MINT32 p2tuningbuf_handle;
  /* VA should be filled by IPC server via search shared memory map table. IPC
   * client is forbidden to fill it */
  MUINTPTR p2tuningbuf_va;
};

}  // namespace NS3Av3

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_IPCISPMGR_H_
