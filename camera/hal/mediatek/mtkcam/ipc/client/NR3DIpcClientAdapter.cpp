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

#undef LOG_TAG
#define LOG_TAG "NR3D_IPC_CLIENT"

#include <cutils/compiler.h>
#include <stdio.h>

#include <camera/hal/mediatek/mtkcam/ipc/client/NR3DIpcClientAdapter.h>

#include "mtkcam/utils/std/Log.h"
#include "Hal3aIpcCommon.h"
#include "IPC3DNR.h"

namespace IPC3DNR {

VISIBILITY_PUBLIC extern "C" MTKEis* createInstance_3DNR_Client(
    const MINT32 sensor_idx, const char* user_name) {
  MY_LOGD("created by user:%s", user_name);

  if (CC_UNLIKELY(sensor_idx >= IPC_MAX_SENSOR_NUM)) {
    MY_LOGE("sensor index %d is illegal, should less than %d", sensor_idx,
            IPC_MAX_SENSOR_NUM);
    return nullptr;
  }
  MY_LOGD("new NR3DIpcClientAdapter");
  NR3DIpcClientAdapter* nr3d_adapter = new NR3DIpcClientAdapter(sensor_idx);
  MY_LOGD("new NR3DIpcClientAdapter out");
  if (CC_UNLIKELY(!nr3d_adapter->m_initialized)) {
    delete nr3d_adapter;
    nr3d_adapter = nullptr;
  }
  MY_LOGD("new NR3DIpcClientAdapter success");
  return static_cast<MTKEis*>(nr3d_adapter);
}

NR3DIpcClientAdapter::NR3DIpcClientAdapter(const MINT32 sensor_idx)
    : m_initialized(false), m_sensor_idx(sensor_idx) {
  mv_mems = {
      {"/mtkNR3D_Create", sizeof(nr3d_create_params), &mMemCreate, false},
      {"/mtkNR3D_Destory", sizeof(nr3d_destory_params), &mMemDestroy, false},
      {"/mtkNR3D_Reset", sizeof(nr3d_reset_params), &mMemReset, false},
      {"/mtkNR3D_Init", sizeof(nr3d_init_params), &mMemInit, false},
      {"/mtkNR3D_Main", sizeof(nr3d_main_params), &mMemMain, false},
      {"/mtkNR3D_FeatureCtrl", sizeof(nr3d_featurectrl_params),
       &mMemFeatureCtrl, false}};

  int ret = false;
  m_ipc_common.init(m_sensor_idx);
  ret = m_ipc_common.allocateAllShmMems(&mv_mems);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("construction fail while allocate ipc shared memroy");
    return;
  }
  MY_LOGD("sendRequest IPC_NR3D_EIS_CREATE in");
  ret = sendRequest(IPC_NR3D_EIS_CREATE, &mMemCreate);
  MY_LOGD("sendRequest IPC_NR3D_EIS_CREATE out");
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("construction fail while create");
    m_ipc_common.releaseAllShmMems(&mv_mems);
    return;
  }
  m_initialized = true;
}

NR3DIpcClientAdapter::~NR3DIpcClientAdapter() {
  m_ipc_common.releaseAllShmMems(&mv_mems);
}

void NR3DIpcClientAdapter::destroyInstance() {
  int ret = false;
  ret = sendRequest(IPC_NR3D_EIS_DESTROY, &mMemDestroy);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("destroy fail");
    return;
  }
}

MRESULT NR3DIpcClientAdapter::EisInit(void* initInData) {
  EIS_SET_ENV_INFO_STRUCT* eisInitInfo;
  eisInitInfo = reinterpret_cast<EIS_SET_ENV_INFO_STRUCT*>(initInData);
  if (eisInitInfo == NULL) {
    MY_LOGE("eis init data is NULL");
    return E_EIS_ERR;
  }
  nr3d_init_params* params = static_cast<nr3d_init_params*>(mMemInit.mAddr);
  memcpy(&params->ipcEisInitData, eisInitInfo, sizeof(EIS_SET_ENV_INFO_STRUCT));
  CheckError(sendRequest(IPC_NR3D_EIS_INIT, &mMemInit) == false,
             FAILED_TRANSACTION, "@%s, EisClientInit fails", __FUNCTION__);
  return S_EIS_OK;
}

MRESULT NR3DIpcClientAdapter::EisMain(EIS_RESULT_INFO_STRUCT* eisResult) {
  MY_LOGD("client: EisMain");
  if (eisResult == NULL) {
    MY_LOGE("eis result is NULL");
    return E_EIS_ERR;
  }
  nr3d_main_params* params = static_cast<nr3d_main_params*>(mMemMain.mAddr);
  CheckError(sendRequest(IPC_NR3D_EIS_MAIN, &mMemMain) == false,
             FAILED_TRANSACTION, "@%s, EisClientMain fails", __FUNCTION__);
  memcpy(eisResult, &params->ipcEisMianData, sizeof(EIS_RESULT_INFO_STRUCT));
  return S_EIS_OK;
}

MRESULT NR3DIpcClientAdapter::EisReset() {
  CheckError(sendRequest(IPC_NR3D_EIS_RESET, &mMemReset) == false,
             FAILED_TRANSACTION, "@%s, EisClientReset fails", __FUNCTION__);
  return S_EIS_OK;
}

MRESULT NR3DIpcClientAdapter::EisFeatureCtrl(MUINT32 FeatureID,
                                             void* pParaIn,
                                             void* pParaOut) {
  nr3d_featurectrl_params* params =
      static_cast<nr3d_featurectrl_params*>(mMemFeatureCtrl.mAddr);

  EIS_SET_PROC_INFO_STRUCT* eisProcInfo;
  EIS_GET_PLUS_INFO_STRUCT* eisPlusData;
  EIS_GMV_INFO_STRUCT* eisOriGmv;

  MY_LOGD("send feature ctrl cmd %d", FeatureID);
  params->eFeatureCtrl = FeatureID;
  switch (FeatureID) {
    case EIS_FEATURE_SET_PROC_INFO:
      eisProcInfo = reinterpret_cast<EIS_SET_PROC_INFO_STRUCT*>(pParaIn);
      if (eisProcInfo == NULL) {
        MY_LOGE("eis proc info is NULL");
        return E_EIS_ERR;
      }
      memcpy(&params->arg.ipcEisProcInfo, eisProcInfo,
             sizeof(EIS_SET_PROC_INFO_STRUCT));
      MY_LOGD("EIS_FEATURE_SET_PROC_INFO DivH:%d", eisProcInfo->DivH);
      MY_LOGD("EIS_FEATURE_SET_PROC_INFO DivV:%d", eisProcInfo->DivV);
      MY_LOGD("EIS_FEATURE_SET_PROC_INFO EisWinNum:%d", eisProcInfo->EisWinNum);
      CheckError(
          sendRequest(IPC_NR3D_EIS_FEATURECTRL, &mMemFeatureCtrl) == false,
          FAILED_TRANSACTION, "@%s, EisClientFeatureCtrl fails", __FUNCTION__);
      break;
    case EIS_FEATURE_GET_EIS_PLUS_DATA:
      eisPlusData = reinterpret_cast<EIS_GET_PLUS_INFO_STRUCT*>(pParaOut);
      if (eisPlusData == NULL) {
        MY_LOGE("eis plus data is NULL");
        return E_EIS_ERR;
      }
      memcpy(&params->arg.ipcEisPlusData, eisPlusData,
             sizeof(EIS_GET_PLUS_INFO_STRUCT));
      CheckError(
          sendRequest(IPC_NR3D_EIS_FEATURECTRL, &mMemFeatureCtrl) == false,
          FAILED_TRANSACTION, "@%s, EisClientFeatureCtrl fails", __FUNCTION__);
      memcpy(eisPlusData, &params->arg.ipcEisPlusData,
             sizeof(EIS_GET_PLUS_INFO_STRUCT));
      MY_LOGD("EIS_FEATURE_GET_EIS_PLUS_DATA GMVx:%f", eisPlusData->GMVx);
      MY_LOGD("EIS_FEATURE_GET_EIS_PLUS_DATA GMVy:%f", eisPlusData->GMVy);
      MY_LOGD("EIS_FEATURE_GET_EIS_PLUS_DATA ConfX:%d", eisPlusData->ConfX);
      MY_LOGD("EIS_FEATURE_GET_EIS_PLUS_DATA ConfY:%d", eisPlusData->ConfY);
      break;
    case EIS_FEATURE_GET_ORI_GMV:
      eisOriGmv = reinterpret_cast<EIS_GMV_INFO_STRUCT*>(pParaOut);
      if (eisOriGmv == NULL) {
        MY_LOGE("eis origin gmv is NULL");
        return E_EIS_ERR;
      }
      memcpy(&params->arg.ipcEisOriGmv, eisOriGmv, sizeof(EIS_GMV_INFO_STRUCT));
      CheckError(
          sendRequest(IPC_NR3D_EIS_FEATURECTRL, &mMemFeatureCtrl) == false,
          FAILED_TRANSACTION, "@%s, EisClientFeatureCtrl fails", __FUNCTION__);
      memcpy(eisOriGmv, &params->arg.ipcEisOriGmv, sizeof(EIS_GMV_INFO_STRUCT));
      MY_LOGD("EIS_FEATURE_GET_ORI_GMV EIS_GMVx:%d", eisOriGmv->EIS_GMVx);
      MY_LOGD("EIS_FEATURE_GET_ORI_GMV EIS_GMVy:%d", eisOriGmv->EIS_GMVy);
      break;
    case EIS_FEATURE_SAVE_LOG:
      CheckError(
          sendRequest(IPC_NR3D_EIS_FEATURECTRL, &mMemFeatureCtrl) == false,
          FAILED_TRANSACTION, "@%s, EisClientFeatureCtrl fails", __FUNCTION__);
      break;
    default:
      break;
  }
  return S_EIS_OK;
}
MINT32 NR3DIpcClientAdapter::sendRequest(IPC_CMD cmd,
                                         ShmMemInfo* meminfo,
                                         int group) {
  MY_LOGD("sendRequest %d", cmd);
  if (meminfo) {
    nr3d_common_params* params =
        static_cast<nr3d_common_params*>(meminfo->mAddr);
    params->sensor_idx = m_sensor_idx;
    return m_ipc_common.requestSync(cmd, meminfo->mHandle, group);
  } else {
    return m_ipc_common.requestSync(cmd);
  }
}

}  // namespace IPC3DNR
