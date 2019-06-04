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

#define LOG_TAG "Mtk3aCommon"

#include <string>
#include <vector>

#include <camera/hal/mediatek/mtkcam/ipc/client/Hal3aIpcCommon.h>

#include "Mediatek3AClient.h"

Mtk3aCommon::Mtk3aCommon() {
  LOG1("@%s", __FUNCTION__);
}

Mtk3aCommon::~Mtk3aCommon() {
  LOG1("@%s", __FUNCTION__);
}

void Mtk3aCommon::init(int i4SensorOpenIndex) {
  LOG1("@%s %d", __FUNCTION__, i4SensorOpenIndex);
  mClient = Mediatek3AClient::getInstance();
  LOG1("@%s, mClient:%p", __FUNCTION__, mClient);
}

bool Mtk3aCommon::allocShmMem(std::string const& name,
                              int size,
                              ShmMemInfo* shm) {
  LOG1("@%s", __FUNCTION__);
  CheckError(mClient == nullptr, false, "@%s, mClient is nullptr",
             __FUNCTION__);

  shm->mName = name;
  shm->mSize = size;
  int ret =
      mClient->allocateShmMem(shm->mName, shm->mSize, &shm->mFd, &shm->mAddr);
  CheckError((ret != OK), false, "@%s, call allocateShmMem fail", __FUNCTION__);

  shm->mHandle = mClient->registerBuffer(shm->mFd);
  CheckError((shm->mHandle < 0), false,
             "@%s, call mBridge->RegisterBuffer fail", __FUNCTION__);

  return true;
}

int32_t Mtk3aCommon::registerBuffer(int bufferFd) {
  return mClient->registerBuffer(bufferFd);
}

void Mtk3aCommon::deregisterBuffer(int32_t bufferHandle) {
  mClient->deregisterBuffer(bufferHandle);
}

bool Mtk3aCommon::requestSync(IPC_CMD cmd, int32_t handle, int32_t group) {
  LOG1("@%s", __FUNCTION__);
  CheckError(mClient == nullptr, false, "@%s, mClient is nullptr",
             __FUNCTION__);

  return mClient->requestSync(cmd, handle, group) == OK ? true : false;
}

bool Mtk3aCommon::requestSync(IPC_CMD cmd, int32_t handle) {
  LOG1("@%s", __FUNCTION__);
  CheckError(mClient == nullptr, false, "@%s, mClient is nullptr",
             __FUNCTION__);

  return mClient->requestSync(cmd, handle) == OK ? true : false;
}

bool Mtk3aCommon::requestSync(IPC_CMD cmd) {
  LOG1("@%s", __FUNCTION__);
  CheckError(mClient == nullptr, false, "@%s, mClient is nullptr",
             __FUNCTION__);

  return mClient->requestSync(cmd) == OK ? true : false;
}

void Mtk3aCommon::freeShmMem(ShmMemInfo* shm) {
  LOG1("@%s, mHandle:%d, mFd:%d, mName:%s, mSize:%d, mAddr:%p", __FUNCTION__,
       shm->mHandle, shm->mFd, shm->mName.c_str(), shm->mSize, shm->mAddr);
  CheckError(mClient == nullptr, VOID_VALUE, "@%s, mClient is nullptr",
             __FUNCTION__);
  if (shm->mHandle < 0 || shm->mFd < 0) {
    IPC_LOGE("@%s, mHandle:%d, mFd:%d, one of them < 0", __FUNCTION__,
             shm->mHandle, shm->mFd);
    return;
  }

  mClient->deregisterBuffer(shm->mHandle);
  mClient->releaseShmMem(shm->mName, shm->mSize, shm->mFd, shm->mAddr);
}

bool Mtk3aCommon::allocateAllShmMems(std::vector<ShmMem>* mems) {
  LOG1("@%s", __FUNCTION__);

  for (std::vector<ShmMem>::iterator it = mems->begin(); it != mems->end();
       it++) {
    ShmMemInfo* mem = it->mem;
    mem->mName = it->name;
    mem->mSize = it->size;
    bool ret = allocShmMem(mem->mName, mem->mSize, mem);
    if (!ret) {
      IPC_LOGE("@%s, allocShmMem fails, name:%s, size:%d", __FUNCTION__,
               mem->mName.c_str(), mem->mSize);
      return false;
    }
    it->allocated = true;
  }

  return true;
}

void Mtk3aCommon::releaseAllShmMems(std::vector<ShmMem>* mems) {
  LOG1("@%s", __FUNCTION__);

  for (std::vector<ShmMem>::iterator it = mems->begin(); it != mems->end();
       it++) {
    if (it->allocated) {
      freeShmMem(it->mem);
      it->allocated = false;
    }
  }
}
