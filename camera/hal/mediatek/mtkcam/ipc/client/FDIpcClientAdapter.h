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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_FDIPCCLIENTADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_FDIPCCLIENTADAPTER_H_

#include <vector>

#include "Hal3aIpcCommon.h"
#include "Mediatek3AClient.h"
#include "IPCFD.h"
#include "IPCCommon.h"

class VISIBILITY_PUBLIC FDIpcClientAdapter {
 public:
  static FDIpcClientAdapter* createInstance(DrvFDObject_e eobject, int openId);
  void destroyInstance();
  ~FDIpcClientAdapter();
  MINT32 FDVTInit(MTKFDFTInitInfo* init_data);
  MINT32 FDVTMain(FdOptions* options, int memFd);
  MINT32 FDGetCalData(fd_cal_struct* fd_cal_data);  // add this two function
  MINT32 FDSetCalData(fd_cal_struct* fd_cal_data);
  MINT32 FDVTMainPhase2();
  MINT32 FDVTReset();
  MINT32 FDVTGetResult(MUINT8* a_FD_ICS_Result,
                       MUINT32 Width,
                       MUINT32 Height,
                       MUINT32 LCM,
                       MUINT32 Sensor,
                       MUINT32 Camera_TYPE,
                       MUINT32 Draw_TYPE);

 private:
  explicit FDIpcClientAdapter(int openId);
  MUINT32 sendRequest(IPC_CMD cmd, ShmMemInfo* memInfo);

 private:
  Mtk3aCommon mCommon;
  bool mInitialized;
  int mOpenId;

  ShmMemInfo mMemCreate;
  ShmMemInfo mMemDestory;
  ShmMemInfo mMemInitInfo;
  ShmMemInfo mMemMainParam;
  ShmMemInfo mMemFdGetCalData;
  ShmMemInfo mMemFdSetCalData;
  ShmMemInfo mMemMainPhase2;
  ShmMemInfo mMemReset;
  ShmMemInfo mMemFdResultInfo;

  std::vector<ShmMem> mMems;
  int32_t mFDBufferHandler;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_FDIPCCLIENTADAPTER_H_
