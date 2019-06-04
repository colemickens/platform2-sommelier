/*
 * Copyright (C) 2017-2019 Intel Corporation
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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_HAL3AIPCCOMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_HAL3AIPCCOMMON_H_

#include <string>
#include <vector>

#include "Mediatek3AClient.h"

typedef struct ShmMemInfo {
  std::string mName;
  int mSize;
  int mFd;
  void* mAddr;
  int32_t mHandle;
  ShmMemInfo() : mName(""), mSize(0), mFd(-1), mAddr(nullptr), mHandle(-1) {}
} ShmMemInfo;

typedef struct ShmMem {
  std::string name;
  int size;
  ShmMemInfo* mem;
  bool allocated;
} ShmMem;

class Mtk3aCommon {
 public:
  Mtk3aCommon();
  virtual ~Mtk3aCommon();

  void init(int i4SensorOpenIndex);
  bool allocShmMem(std::string const& name, int size, ShmMemInfo* shm);
  bool requestSync(IPC_CMD cmd, int32_t handle, int32_t group);
  bool requestSync(IPC_CMD cmd, int32_t handle);
  bool requestSync(IPC_CMD cmd);
  void freeShmMem(ShmMemInfo* shm);

  bool allocateAllShmMems(std::vector<ShmMem>* mems);
  void releaseAllShmMems(std::vector<ShmMem>* mems);

  int32_t registerBuffer(int bufferFd);

  void deregisterBuffer(int32_t bufferHandle);

 private:
  Mediatek3AClient* mClient;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_HAL3AIPCCOMMON_H_
