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
#define LOG_TAG "NR3D_IPC_SERVER"

#include <cutils/compiler.h>
#include <stdio.h>

#include <camera/hal/mediatek/mtkcam/ipc/server/NR3DIpcServerAdapter.h>

#include "mtkcam/utils/std/Log.h"
#include "IPC3DNR.h"
#include "Errors.h"

namespace IPC3DNR {

int NR3DIpcServerAdapter::create(void* addr, int dataSize) {
  CheckError((dataSize < sizeof(nr3d_create_params)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  MY_LOGD("m_pEisAlg create");
  if (m_pEisAlg == NULL) {
    m_pEisAlg = MTKEis::createInstance();
    if (m_pEisAlg == NULL) {
      MY_LOGE("MTKEis::createInstance fail");
      return UNKNOWN_ERROR;
    }
  }
  return OK;
}

int NR3DIpcServerAdapter::destroy(void* addr, int dataSize) {
  CheckError((dataSize < sizeof(nr3d_destory_params)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  if (m_pEisAlg != NULL) {
    MY_LOGD("m_pEisAlg uninit");
    m_pEisAlg->destroyInstance();
    m_pEisAlg = NULL;
  }
  return OK;
}

int NR3DIpcServerAdapter::init(void* addr, int dataSize) {
  CheckError((dataSize < sizeof(nr3d_init_params)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  nr3d_init_params* init_params = static_cast<nr3d_init_params*>(addr);
  if (!init_params) {
    MY_LOGE("NR3D Init Buffer is NULL");
    return -1;
  }
  if (m_pEisAlg->EisInit(&init_params->ipcEisInitData) != S_EIS_OK) {
    MY_LOGE("EisInit fail");
    return UNKNOWN_ERROR;
  }
  return OK;
}

int NR3DIpcServerAdapter::main(void* addr, int dataSize) {
  CheckError((dataSize < sizeof(nr3d_main_params)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  nr3d_main_params* main_params = static_cast<nr3d_main_params*>(addr);
  if (!main_params) {
    MY_LOGE("NR3D Main Buffer is NULL");
    return -1;
  }
  if (m_pEisAlg->EisMain(&main_params->ipcEisMianData) != S_EIS_OK) {
    MY_LOGE("EisAlg:EisMain fail");
    return UNKNOWN_ERROR;
  }
  return OK;
}

int NR3DIpcServerAdapter::reset(void* addr, int dataSize) {
  CheckError((dataSize < sizeof(nr3d_reset_params)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  if (m_pEisAlg != NULL) {
    MY_LOGD("m_pEisAlg reset");
    m_pEisAlg->EisReset();
  }
  return OK;
}

int NR3DIpcServerAdapter::featureCtrl(void* addr, int dataSize) {
  CheckError((dataSize < sizeof(nr3d_featurectrl_params)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  nr3d_featurectrl_params* params = static_cast<nr3d_featurectrl_params*>(addr);
  if (!params) {
    MY_LOGE("NR3D Feature Ctrl Buffer is NULL");
    return -1;
  }
  switch (params->eFeatureCtrl) {
    case EIS_FEATURE_SET_PROC_INFO:
      if (m_pEisAlg->EisFeatureCtrl(EIS_FEATURE_SET_PROC_INFO,
                                    &params->arg.ipcEisProcInfo,
                                    NULL) != S_EIS_OK) {
        MY_LOGE("EisAlg:LMV_FEATURE_SET_PROC_INFO fail");
        return UNKNOWN_ERROR;
      }
      MY_LOGD("server:EIS_FEATURE_SET_PROC_INFO DivH:%d",
              params->arg.ipcEisProcInfo.DivH);
      MY_LOGD("server:EIS_FEATURE_SET_PROC_INFO DivV:%d",
              params->arg.ipcEisProcInfo.DivV);
      MY_LOGD("server:EIS_FEATURE_SET_PROC_INFO EisWinNum:%d",
              params->arg.ipcEisProcInfo.EisWinNum);
      break;
    case EIS_FEATURE_GET_EIS_PLUS_DATA:
      EIS_GET_PLUS_INFO_STRUCT* eisPlusInfo;
      if (m_pEisAlg->EisFeatureCtrl(EIS_FEATURE_GET_EIS_PLUS_DATA, NULL,
                                    &params->arg.ipcEisPlusData) != S_EIS_OK) {
        MY_LOGE("EisAlg:LMV_FEATURE_GET_LMV_PLUS_DATA fail");
        return UNKNOWN_ERROR;
      }
      eisPlusInfo = reinterpret_cast<EIS_GET_PLUS_INFO_STRUCT*>(
          &params->arg.ipcEisPlusData);
      memcpy(&params->arg.ipcEisPlusData, eisPlusInfo,
             sizeof(EIS_GET_PLUS_INFO_STRUCT));
      MY_LOGD("server:EIS_FEATURE_GET_EIS_PLUS_DATA GMVx:%f",
              eisPlusInfo->GMVx);
      MY_LOGD("server:EIS_FEATURE_GET_EIS_PLUS_DATA GMVy:%f",
              eisPlusInfo->GMVy);
      MY_LOGD("server:EIS_FEATURE_GET_EIS_PLUS_DATA ConfX:%d",
              eisPlusInfo->ConfX);
      MY_LOGD("server:EIS_FEATURE_GET_EIS_PLUS_DATA ConfY:%d",
              eisPlusInfo->ConfY);
      break;
    case EIS_FEATURE_GET_ORI_GMV:
      if (m_pEisAlg->EisFeatureCtrl(EIS_FEATURE_GET_ORI_GMV, NULL,
                                    &params->arg.ipcEisOriGmv) != S_EIS_OK) {
        MY_LOGE("EisAlg:LMV_FEATURE_GET_ORI_GMV fail");
        return UNKNOWN_ERROR;
      }
      MY_LOGD("server:EIS_FEATURE_GET_ORI_GMV EIS_GMVx:%d",
              params->arg.ipcEisOriGmv.EIS_GMVx);
      MY_LOGD("server:EIS_FEATURE_GET_ORI_GMV EIS_GMVy:%d",
              params->arg.ipcEisOriGmv.EIS_GMVy);
      break;
    case EIS_FEATURE_SAVE_LOG:
      if (m_pEisAlg->EisFeatureCtrl(EIS_FEATURE_SAVE_LOG, NULL, NULL) !=
          S_EIS_OK) {
        MY_LOGE("EisFeatureCtrl(EIS_FEATURE_SAVE_LOG) fail");
      }
      break;
    default:
      MY_LOGE("Unsupported Eis Feature Ctrl Type");
      return -1;
      break;
  }
  return OK;
}

}  // namespace IPC3DNR
