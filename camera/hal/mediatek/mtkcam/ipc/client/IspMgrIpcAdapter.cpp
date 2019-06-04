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

#define LOG_TAG "IspMgrIpcAdapterClient"

#include <stdio.h>
#include <string>

#include <camera/hal/mediatek/mtkcam/ipc/client/IspMgrIpcAdapter.h>

#include <mtkcam/aaa/aaa_utils.h>
#include <MyUtils.h>
#include "mtkcam/aaa/IIspMgr.h"
#include "Hal3aIpcCommon.h"
#include "IPCIspMgr.h"

namespace NS3Av3 {

VISIBILITY_PUBLIC extern "C" IIspMgrIPC* getInstance_ispMgr_IPC(
    const char* strUser) {
  static IspMgrIpcAdapter* _singleton = new IspMgrIpcAdapter;
  _singleton->init(strUser);
  return _singleton;
}

void IspMgrIpcAdapter::init(const char* strUser) {
  std::lock_guard<std::mutex> lock(mInitMutex);

  MY_LOGD("[%s] User.count(%d), User init(%s)", __FUNCTION__, m_Users.size(),
          strUser);

  if (mInitialized) {
    m_Users[string(strUser)]++;
    return;
  }

  imMem = {
      {"/mtkIspMgrCreate", sizeof(ispmgr_create_params), &imMemCreate, false},
      {"/mtkIspMgrQueryLCSO", sizeof(ispmgr_querylcso_params), &imMemQueryLCSO,
       false},
      {"/mtkIspMgrPostProcessNR3D", sizeof(ispmgr_ppnr3d_params), &imMemPPNR3D,
       false},
  };

  bool ret = false;
  mCommon.init(0);
  ret = mCommon.allocateAllShmMems(&imMem);
  if (!ret) {
    LOGE("Allocate shared memory failed!");
    mCommon.releaseAllShmMems(&imMem);
    return;
  }

  ret = sendRequest(IPC_ISPMGR_CREATE, &imMemCreate);
  if (!ret) {
    LOGE("construction fail while create");
    mCommon.releaseAllShmMems(&imMem);
    return;
  }

  m_Users[string(strUser)]++;

  mInitialized = true;
}

void IspMgrIpcAdapter::uninit(const char* strUser) {
  std::lock_guard<std::mutex> lock(mInitMutex);

  MY_LOGD("[%s] User.count(%d), User uninit(%s)", __FUNCTION__, m_Users.size(),
          strUser);

  if (!m_Users[string(strUser)]) {
    CAM_LOGE("User(%s) did not create IspMgr!", strUser);
  } else {
    m_Users[string(strUser)]--;
    if (!m_Users[string(strUser)]) {
      m_Users.erase(string(strUser));

      if (!m_Users.size()) {
        for (const auto& n : m_map_p2tuningbuf_handles) {
          mCommon.deregisterBuffer(n.second);
        }
        m_map_p2tuningbuf_handles.clear();
        mCommon.releaseAllShmMems(&imMem);
        mInitialized = false;
      } else {
        MY_LOGD("[%s] Still %d users", __FUNCTION__, m_Users.size());
      }
    }
  }

  MY_LOGD("[%s] - User.count(%d)", __FUNCTION__, m_Users.size());
}

IspMgrIpcAdapter::IspMgrIpcAdapter() {
  mInitialized = false;
}

IspMgrIpcAdapter::~IspMgrIpcAdapter() {
  if (mInitialized)
    mCommon.releaseAllShmMems(&imMem);
}

MVOID IspMgrIpcAdapter::queryLCSOParams(LCSO_Param* param) {
  ispmgr_querylcso_params* params =
      static_cast<ispmgr_querylcso_params*>(imMemQueryLCSO.mAddr);

  int ret = sendRequest(IPC_ISPMGR_QUERYLCSO, &imMemQueryLCSO);
  if (!ret) {
    LOGE("sync request fail with error status %d", ret);
    return;
  }

  param->size.w = params->lcsoParam.size.w;
  param->size.h = params->lcsoParam.size.h;
  param->format = params->lcsoParam.format;
  param->stride = params->lcsoParam.stride;
  param->bitDepth = params->lcsoParam.bitDepth;

  return;
}

MVOID IspMgrIpcAdapter::postProcessNR3D(MINT32 sensorIndex,
                                        NR3D_Config_Param* param,
                                        void* pTuning) {
  ispmgr_ppnr3d_params* params =
      static_cast<ispmgr_ppnr3d_params*>(imMemPPNR3D.mAddr);

  params->nr3dParams.enable = param->enable;
  /* region modified by GMV */
  params->nr3dParams.onRegion.p.x = param->onRegion.p.x;
  params->nr3dParams.onRegion.p.y = param->onRegion.p.y;
  params->nr3dParams.onRegion.s.w = param->onRegion.s.w;
  params->nr3dParams.onRegion.s.h = param->onRegion.s.h;
  /* image full size for demo mode calculation */
  params->nr3dParams.fullImg.p.x = param->fullImg.p.x;
  params->nr3dParams.fullImg.p.y = param->fullImg.p.y;
  params->nr3dParams.fullImg.s.w = param->fullImg.s.w;
  params->nr3dParams.fullImg.s.h = param->fullImg.s.h;
  /* vipi */
  params->nr3dParams.vipiOffst = param->vipiOffst;
  params->nr3dParams.vipiReadSize.w = param->vipiReadSize.w;
  params->nr3dParams.vipiReadSize.h = param->vipiReadSize.h;

  /* tuning parameters */
  int64_t tuning_fd = reinterpret_cast<int64_t>(pTuning);
  if (tuning_fd <= 0) {
    LOGE("postProcessNR3D : Tuning Parameter Buffer is NULL");
    return;
  }

  auto it_p2tuningbuf_handle = m_map_p2tuningbuf_handles.find(tuning_fd);
  if (it_p2tuningbuf_handle == m_map_p2tuningbuf_handles.end()) {
    int32_t buff_handle = mCommon.registerBuffer(tuning_fd);
    if (buff_handle < 0) {
      IPC_LOGE("register p2 tuning buffer fail");
      return;
    }
    auto pair = m_map_p2tuningbuf_handles.emplace(tuning_fd, buff_handle);
    it_p2tuningbuf_handle = pair.first;
  }
  params->p2tuningbuf_handle = it_p2tuningbuf_handle->second;

  int ret = sendRequest(IPC_ISPMGR_PPNR3D, &imMemPPNR3D);
  if (!ret) {
    LOGE("sync request fail with error status %d", ret);
    return;
  }

  return;
}

MINT32 IspMgrIpcAdapter::sendRequest(IPC_CMD cmd,
                                     ShmMemInfo* memInfo,
                                     int32_t group) {
  return mCommon.requestSync(cmd, memInfo->mHandle, group);
}

}  // namespace NS3Av3
