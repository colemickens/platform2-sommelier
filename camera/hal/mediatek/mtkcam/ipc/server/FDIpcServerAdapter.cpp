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
#define LOG_TAG "FD_IPC_SERVER"

#include <cutils/compiler.h>
#include <faces.h>
#include <memory>
#include <MTKDetection.h>
#include <pthread.h>
#include <stdio.h>

#include <camera/hal/mediatek/mtkcam/ipc/server/FDIpcServerAdapter.h>

#include "mtkcam/utils/std/Log.h"
#include "IPCFD.h"
#include "Errors.h"

void LockFTBuffer(void* arg) {
  MY_LOGD("LockFTBuffer");
}
void UnlockFTBuffer(void* arg) {
  MY_LOGD("UnlockFTBuffer");
}

FDIpcServerAdapter::FDIpcServerAdapter()
    : mpFdCalData(nullptr), mBufVa(nullptr), mpImageScaleBuffer(nullptr) {
  TRACE_FUNC_ENTER();
  mFDWorkingBufferSize = 4194304;  // 4M: 1024*1024*4
  mFDWorkingBuffer = std::make_unique<unsigned char[]>(mFDWorkingBufferSize);
  MtkCameraFaceMetadata* facesInput = new MtkCameraFaceMetadata;
  if (nullptr != facesInput) {
    MtkCameraFace* faces = new MtkCameraFace[FD_MAX_FACE_NUM];
    MtkFaceInfo* posInfo = new MtkFaceInfo[FD_MAX_FACE_NUM];

    if (nullptr != faces && nullptr != posInfo) {
      facesInput->faces = faces;
      facesInput->posInfo = posInfo;
      facesInput->number_of_faces = 0;
    } else {
      MY_LOGE("Fail to allocate Faceinfo buffer");
    }
  } else {
    MY_LOGE("Fail to allocate FaceMetadata buffer");
  }
  mpDetectedFaces.reset(facesInput);
  TRACE_FUNC_EXIT();
}

FDIpcServerAdapter::~FDIpcServerAdapter() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

int FDIpcServerAdapter::create(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  if ((addr == nullptr) || (sizeof(FDCreateInfo) != dataSize)) {
    MY_LOGE("addr is null, or dataSize is false");
    return UNKNOWN_ERROR;
  }

  FDCreateInfo* Params = static_cast<FDCreateInfo*>(addr);  // mode
  mpMTKFDVTObj = MTKDetection::createInstance((DrvFDObject_e)(Params->FDMode));
  if (mpMTKFDVTObj == nullptr) {
    MY_LOGD("mpMTKFDVTObj is null");
    return UNKNOWN_ERROR;
  }
  TRACE_FUNC_EXIT();
  return OK;
}

int FDIpcServerAdapter::destory(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  if (addr == nullptr) {
    MY_LOGE("addr is null");
    return UNKNOWN_ERROR;
  }

  mpMTKFDVTObj->destroyInstance();
  if (mBufVa != nullptr) {
    mBufVa = nullptr;
  }
  TRACE_FUNC_EXIT();
  return OK;
}

int FDIpcServerAdapter::init(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  if ((addr == nullptr) || (sizeof(FDInitInfo) != dataSize)) {
    MY_LOGE("addr is null, or dataSize is false");
    return UNKNOWN_ERROR;
  }
  FDInitInfo* Params = static_cast<FDInitInfo*>(addr);

  int mImageScaleTotalSize = 0;
  for (int i = 0; i < FD_SCALE_NUM; i++) {
    mImage_width_array[i] = Params->FDImageWidthArray[i];
    mImage_height_array[i] = Params->FDImageHeightArray[i];
    mImageScaleTotalSize +=
        Params->FDImageWidthArray[i] * Params->FDImageHeightArray[i];
  }

  mpImageScaleBuffer = std::make_unique<unsigned char[]>(mImageScaleTotalSize);
  MTKFDFTInitInfo FDFTInitInfo;
  memcpy(&FDFTInitInfo, &(Params->initInfo), sizeof(FDIPCInitInfo));
  FDFTInitInfo.FDImageWidthArray = mImage_width_array;
  FDFTInitInfo.FDImageHeightArray = mImage_width_array;
  FDFTInitInfo.WorkingBufAddr = mFDWorkingBuffer.get();
  FDFTInitInfo.WorkingBufSize = mFDWorkingBufferSize;
  FDFTInitInfo.LockOtBufferFunc = LockFTBuffer;
  FDFTInitInfo.UnlockOtBufferFunc = UnlockFTBuffer;
  mpMTKFDVTObj->FDVTInit(&FDFTInitInfo);
  TRACE_FUNC_EXIT();
  return OK;
}

int FDIpcServerAdapter::main(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  if ((addr == nullptr) || (sizeof(FDMainParam) != dataSize)) {
    MY_LOGE("addr is null, or dataSize is false");
    return UNKNOWN_ERROR;
  }
  FDMainParam* Params = static_cast<FDMainParam*>(addr);
  FdOptions FDOps;
  if (mBufVa == nullptr) {
    mBufVa = reinterpret_cast<MUINT8*>((Params->common).bufferva);
    MY_LOGD("mBufVa = %p", mBufVa);
  }
  memcpy(&FDOps, &(Params->mainParam), sizeof(FDIPCMainParam));
  FDOps.ImageBufferPhyPlane1 = nullptr;
  FDOps.ImageBufferPhyPlane2 = nullptr;
  FDOps.ImageBufferPhyPlane3 = nullptr;
  FDOps.ImageScaleBuffer = mpImageScaleBuffer.get();
  FDOps.ImageBufferRGB565 = mBufVa;
  FDOps.ImageBufferSrcVirtual = mBufVa;
  mpMTKFDVTObj->FDVTMain(&FDOps);
  (Params->mainParam).doPhase2 = FDOps.doPhase2;
  TRACE_FUNC_EXIT();
  return OK;
}

int FDIpcServerAdapter::getCalData(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  if ((addr == nullptr) || (sizeof(FDCalData) != dataSize)) {
    MY_LOGE("addr is null, or dataSize is false");
    return UNKNOWN_ERROR;
  }
  FDCalData* Params = static_cast<FDCalData*>(addr);
  mpFdCalData = mpMTKFDVTObj->FDGetCalData();
  if (mpFdCalData == nullptr) {
    MY_LOGE("FDGetCalData fail.");
    return UNKNOWN_ERROR;
  }
  memcpy(&(Params->calData), mpFdCalData, sizeof(FDIPCCalData));
  TRACE_FUNC_EXIT();
  return OK;
}

int FDIpcServerAdapter::setCalData(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  if ((addr == nullptr) || (sizeof(FDCalData) != dataSize)) {
    MY_LOGE("addr is null, or dataSize is false");
    return UNKNOWN_ERROR;
  }
  FDCalData* Params = static_cast<FDCalData*>(addr);
  memcpy(mpFdCalData, &(Params->calData), sizeof(FDIPCCalData));
  TRACE_FUNC_EXIT();
  return OK;
}

int FDIpcServerAdapter::mainPhase2(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  if (addr == nullptr) {
    MY_LOGE("addr is null");
    return UNKNOWN_ERROR;
  }
  mpMTKFDVTObj->FDVTMainPhase2();
  TRACE_FUNC_EXIT();
  return OK;
}

int FDIpcServerAdapter::getResult(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  if ((addr == nullptr) || (sizeof(FDGetResultInfo) != dataSize)) {
    MY_LOGE("addr is null, or dataSize is false");
    return UNKNOWN_ERROR;
  }
  int width;
  int height;
  int faceCnt;
  result pbuf[FD_MAX_FACE_NUM];
  FDGetResultInfo* Params = static_cast<FDGetResultInfo*>(addr);
  width = Params->width;
  height = Params->height;

  faceCnt = mpMTKFDVTObj->FDVTGetResult(reinterpret_cast<MUINT8*>(pbuf),
                                        FACEDETECT_TRACKING_DISPLAY);
  mpMTKFDVTObj->FDVTGetICSResult(
      reinterpret_cast<MUINT8*>(mpDetectedFaces.get()),
      reinterpret_cast<MUINT8*>(pbuf), width, height, 0, 0, 0, 0);

  MY_LOGD("face number = %d", faceCnt);
  memcpy(&((Params->FaceResult).result), mpDetectedFaces.get(),
         sizeof(FDIPCResult));
  memcpy(&((Params->FaceResult).faces), mpDetectedFaces->faces,
         sizeof(MtkCameraFace) * FD_MAX_FACE_NUM);
  memcpy(&((Params->FaceResult).posInfo), mpDetectedFaces->posInfo,
         sizeof(MtkFaceInfo) * FD_MAX_FACE_NUM);
  TRACE_FUNC_EXIT();
  return OK;
}

int FDIpcServerAdapter::reset(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  if (addr == nullptr) {
    MY_LOGE("addr is null");
    return UNKNOWN_ERROR;
  }
  mpMTKFDVTObj->FDVTReset();
  TRACE_FUNC_EXIT();
  return OK;
}
