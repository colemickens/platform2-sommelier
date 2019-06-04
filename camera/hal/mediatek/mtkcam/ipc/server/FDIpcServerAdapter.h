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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_FDIPCSERVERADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_FDIPCSERVERADAPTER_H_

#include <memory>
#include "IPCCommon.h"
#include "IPCFD.h"

struct CameraFacesDeleter {
  inline void operator()(MtkCameraFaceMetadata* faces) {
    // Delete face metadata buffer
    if (faces != nullptr) {
      if (faces->faces != nullptr) {
        delete[] faces->faces;
      }
      if (faces->posInfo != nullptr) {
        delete[] faces->posInfo;
      }
      delete faces;
    }
  }
};

typedef std::unique_ptr<MtkCameraFaceMetadata, struct CameraFacesDeleter>
    CameraFacesUniquePtr;

class FDIpcServerAdapter {
 public:
  FDIpcServerAdapter();
  ~FDIpcServerAdapter();
  int create(void* addr, int dataSize);
  int destory(void* addr, int dataSize);
  int init(void* addr, int dataSize);
  int main(void* addr, int dataSize);
  int getCalData(void* addr, int dataSize);
  int setCalData(void* addr, int dataSize);
  int mainPhase2(void* addr, int dataSize);
  int getResult(void* addr, int dataSize);
  int reset(void* addr, int dataSize);

 private:
  MTKDetection* mpMTKFDVTObj;
  fd_cal_struct* mpFdCalData;
  MUINT8* mBufVa;
  MUINT32 mFDWorkingBufferSize;
  std::unique_ptr<MUINT8[]> mFDWorkingBuffer;
  std::unique_ptr<MUINT8[]> mpImageScaleBuffer;
  CameraFacesUniquePtr mpDetectedFaces;
  MUINT32 mImage_width_array[FD_SCALE_NUM];
  MUINT32 mImage_height_array[FD_SCALE_NUM];
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_FDIPCSERVERADAPTER_H_
