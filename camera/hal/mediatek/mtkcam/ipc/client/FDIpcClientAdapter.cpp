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
#define LOG_TAG "FD_IPC_CLIENT"

#include <cutils/compiler.h>
#include <stdio.h>
#include <string.h>

#include <camera/hal/mediatek/mtkcam/ipc/client/FDIpcClientAdapter.h>

#include "mtkcam/utils/std/Log.h"
#include "Hal3aIpcCommon.h"
#include "IPCFD.h"

FDIpcClientAdapter* FDIpcClientAdapter::createInstance(DrvFDObject_e eobject,
                                                       int openId) {
  TRACE_FUNC_ENTER();
  FDIpcClientAdapter* fd_adapter = new FDIpcClientAdapter(openId);

  if (fd_adapter == nullptr) {
    MY_LOGE("fd_adapter create fail");
  }

  if (CC_UNLIKELY(!fd_adapter->mInitialized)) {
    MY_LOGE("fd_adapter mInitialized fail");
    delete fd_adapter;
    fd_adapter = nullptr;
  }

  TRACE_FUNC_EXIT();
  return fd_adapter;
}

void FDIpcClientAdapter::destroyInstance() {
  TRACE_FUNC_ENTER();
  int ret = sendRequest(IPC_FD_DESTORY, &mMemDestory);
  if (ret == false) {
    MY_LOGE("FD destroyInstance failed");
  }
  delete this;
  TRACE_FUNC_EXIT();
  return;
}

FDIpcClientAdapter::FDIpcClientAdapter(int openId)
    : mInitialized(false), mFDBufferHandler(0) {
  TRACE_FUNC_ENTER();
  int ret;
  FDCreateInfo* Params;
  mOpenId = openId;
  mMems = {
      {"/mtkFDCreate", sizeof(FDCreateInfo), &mMemCreate, false},
      {"/mtkFDDestory", sizeof(FDDestoryInfo), &mMemDestory, false},
      {"/mtkFDInit", sizeof(FDInitInfo), &mMemInitInfo, false},
      {"/mtkFDMain", sizeof(FDMainParam), &mMemMainParam, false},
      {"/mtkFDGetCalData", sizeof(FDCalData), &mMemFdGetCalData, false},
      {"/mtkFDSetCalData", sizeof(FDCalData), &mMemFdSetCalData, false},
      {"/mtkFDMainPhase2", sizeof(FDMainPhase2), &mMemMainPhase2, false},
      {"/mtkFDReset", sizeof(FDReset), &mMemReset, false},
      {"/mtkFDResult", sizeof(FDGetResultInfo), &mMemFdResultInfo, false},
  };

  mCommon.init(mOpenId);

  bool success = mCommon.allocateAllShmMems(&mMems);
  if (!success) {
    mCommon.releaseAllShmMems(&mMems);
    return;
  }

  Params = static_cast<FDCreateInfo*>(mMemCreate.mAddr);
  Params->FDMode = DRV_FD_OBJ_HW;
  ret = sendRequest(IPC_FD_CREATE, &mMemCreate);
  if (ret == false) {
    MY_LOGE("FD init failed");
    return;
  }

  mInitialized = true;
  TRACE_FUNC_EXIT();
}

FDIpcClientAdapter::~FDIpcClientAdapter() {
  TRACE_FUNC_ENTER();
  mCommon.deregisterBuffer(mFDBufferHandler);
  mCommon.releaseAllShmMems(&mMems);
  TRACE_FUNC_EXIT();
}

MINT32 FDIpcClientAdapter::FDVTInit(MTKFDFTInitInfo* init_data) {
  TRACE_FUNC_ENTER();
  if (init_data == nullptr) {
    MY_LOGE("init_data is null");
    return -1;
  }

  if (mMemInitInfo.mAddr == nullptr) {
    MY_LOGE("mMemInitInfo.mAddr == nullptr");
  }
  FDInitInfo* Params = static_cast<FDInitInfo*>(mMemInitInfo.mAddr);
  memcpy(&(Params->initInfo), init_data, sizeof(FDIPCInitInfo));
  for (int i = 0; i < FD_SCALE_NUM; i++) {
    Params->FDImageWidthArray[i] = init_data->FDImageWidthArray[i];
    Params->FDImageHeightArray[i] = init_data->FDImageHeightArray[i];
  }
  CheckError(sendRequest(IPC_FD_INIT, &mMemInitInfo) == false,
             FAILED_TRANSACTION, "@%s, requestSync fails", __FUNCTION__);
  TRACE_FUNC_EXIT();
  return 0;
}

MINT32 FDIpcClientAdapter::FDVTMain(FdOptions* options, int memFd) {
  TRACE_FUNC_ENTER();
  FDMainParam* Params = static_cast<FDMainParam*>(mMemMainParam.mAddr);
  if (mFDBufferHandler == 0) {
    mFDBufferHandler = mCommon.registerBuffer(memFd);
    MY_LOGD("mFDBufferHandler = %d", mFDBufferHandler);
  }
  memcpy(&(Params->mainParam), options, sizeof(FDIPCMainParam));
  Params->fdBuffer = mFDBufferHandler;
  CheckError(sendRequest(IPC_FD_MAIN, &mMemMainParam) == false,
             FAILED_TRANSACTION, "@%s, requestSync fails", __FUNCTION__);
  options->doPhase2 = (Params->mainParam).doPhase2;
  TRACE_FUNC_EXIT();
  return 0;
}
MINT32 FDIpcClientAdapter::FDGetCalData(fd_cal_struct* fd_cal_data) {
  TRACE_FUNC_ENTER();
  FDCalData* Params = static_cast<FDCalData*>(mMemFdGetCalData.mAddr);
  CheckError(sendRequest(IPC_FD_GET_CAL_DATA, &mMemFdGetCalData) == false,
             FAILED_TRANSACTION, "@%s, requestSync fails", __FUNCTION__);
  memcpy(fd_cal_data, &(Params->calData), sizeof(FDIPCCalData));
  TRACE_FUNC_EXIT();
  return 0;
}

MINT32 FDIpcClientAdapter::FDSetCalData(fd_cal_struct* fd_cal_data) {
  TRACE_FUNC_ENTER();
  FDCalData* Params = static_cast<FDCalData*>(mMemFdSetCalData.mAddr);
  memcpy(&(Params->calData), fd_cal_data, sizeof(FDIPCCalData));
  CheckError(sendRequest(IPC_FD_SET_CAL_DATA, &mMemFdSetCalData) == false,
             FAILED_TRANSACTION, "@%s, requestSync fails", __FUNCTION__);
  return 0;
}

MINT32 FDIpcClientAdapter::FDVTMainPhase2() {
  TRACE_FUNC_ENTER();
  CheckError(sendRequest(IPC_FD_MAIN_PHASE2, &mMemMainPhase2) == false,
             FAILED_TRANSACTION, "@%s, requestSync fails", __FUNCTION__);
  TRACE_FUNC_EXIT();
  return 0;
}

MINT32 FDIpcClientAdapter::FDVTReset() {
  TRACE_FUNC_ENTER();
  CheckError(sendRequest(IPC_FD_RESET, &mMemReset) == false, FAILED_TRANSACTION,
             "@%s, requestSync fails", __FUNCTION__);
  TRACE_FUNC_EXIT();
  return 0;
}

MINT32 FDIpcClientAdapter::FDVTGetResult(MUINT8* a_FD_ICS_Result,
                                         MUINT32 Width,
                                         MUINT32 Height,
                                         MUINT32 LCM,
                                         MUINT32 Sensor,
                                         MUINT32 Camera_TYPE,
                                         MUINT32 Draw_TYPE) {
  TRACE_FUNC_ENTER();
  FDGetResultInfo* Params =
      static_cast<FDGetResultInfo*>(mMemFdResultInfo.mAddr);
  Params->width = Width;
  Params->height = Height;
  CheckError(sendRequest(IPC_FD_GETRESULT, &mMemFdResultInfo) == false,
             FAILED_TRANSACTION, "@%s, requestSync fails", __FUNCTION__);
  memcpy(reinterpret_cast<MtkCameraFaceMetadata*>(a_FD_ICS_Result),
         &((Params->FaceResult).result), sizeof(FDIPCResult));
  memcpy((reinterpret_cast<MtkCameraFaceMetadata*>(a_FD_ICS_Result))->faces,
         &((Params->FaceResult).faces),
         sizeof(MtkCameraFace) * FD_MAX_FACE_NUM);
  memcpy((reinterpret_cast<MtkCameraFaceMetadata*>(a_FD_ICS_Result))->posInfo,
         &((Params->FaceResult).posInfo),
         sizeof(MtkFaceInfo) * FD_MAX_FACE_NUM);
  TRACE_FUNC_EXIT();
  return 0;
}

MUINT32 FDIpcClientAdapter::sendRequest(IPC_CMD cmd, ShmMemInfo* memInfo) {
  FDCommonParams* params = static_cast<FDCommonParams*>(memInfo->mAddr);
  params->m_i4SensorIdx = mOpenId;

  return mCommon.requestSync(cmd, memInfo->mHandle, IPC_GROUP_FD);
}
