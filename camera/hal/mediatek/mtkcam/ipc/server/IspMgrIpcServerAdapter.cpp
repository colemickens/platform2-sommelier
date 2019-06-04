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

#define LOG_TAG "IspMgrIpcAdapterServer"

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <Errors.h>

#include <camera/hal/mediatek/mtkcam/ipc/server/IspMgrIpcServerAdapter.h>

#include "IPCIspMgr.h"
#include "IPCCommon.h"
#include "Mediatek3AServer.h"

namespace NS3Av3 {

int IspMgrIpcServerAdapter::create(void* addr, int dataSize) {
  std::lock_guard<std::mutex> _lk(create_lock);
  CheckError((dataSize < sizeof(ispmgr_create_params)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);

  if (msp_ispmgr == nullptr)
    msp_ispmgr = MAKE_IspMgr_IPC();

  return OK;
}

int IspMgrIpcServerAdapter::querylcso(void* addr, int dataSize) {
  CheckError((dataSize < sizeof(ispmgr_querylcso_params)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);

  ispmgr_querylcso_params* params = static_cast<ispmgr_querylcso_params*>(addr);
  NS3Av3::LCSO_Param lcsoParam;
  if (!params) {
    IPC_LOGE("Query Lcso Buffer is NULL");
    return -1;
  }

  msp_ispmgr->queryLCSOParams(&lcsoParam);

  params->lcsoParam.size.w = lcsoParam.size.w;
  params->lcsoParam.size.h = lcsoParam.size.h;
  params->lcsoParam.format = lcsoParam.format;
  params->lcsoParam.stride = lcsoParam.stride;
  params->lcsoParam.bitDepth = lcsoParam.bitDepth;

  return OK;
}

int IspMgrIpcServerAdapter::ppnr3d(void* addr, int dataSize) {
  CheckError((dataSize < sizeof(ispmgr_ppnr3d_params)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  ispmgr_ppnr3d_params* params = static_cast<ispmgr_ppnr3d_params*>(addr);
  if (!params) {
    IPC_LOGE("Post Process NR3D Buffer is NULL");
    return -1;
  }

  msp_ispmgr->postProcessNR3D(params->sensorIdx, &(params->nr3dParams),
                              reinterpret_cast<void*>(params->p2tuningbuf_va));

  return OK;
}

}  // namespace NS3Av3
