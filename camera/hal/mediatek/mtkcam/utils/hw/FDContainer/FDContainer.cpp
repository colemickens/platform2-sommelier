/*
 * Copyright (C) 2019 MediaTek Inc.
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

#define LOG_TAG "MtkCam/HwUtils/FDContainer"

#include <cutils/compiler.h>
#include <FDContainer/FDContainer.h>

#include <map>

#include <mtkcam/def/common.h>
#include <mtkcam/utils/hw/FleetingQueue.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/StlUtils.h>

#include <string>
#include <unordered_set>
#include <vector>

using NSCam::IFDContainer;

#ifndef TIME_LIMIT_QUEUE_SIZE
#define TIME_LIMIT_QUEUE_SIZE 26
#endif
#ifndef TIME_LIMIT_QUEUE_BACKUP_SIZE
#define TIME_LIMIT_QUEUE_BACKUP_SIZE 4
#endif

// at least one block for query.
#if (TIME_LIMIT_QUEUE_SIZE <= 0)
#error "TIME_LIMIT_QUEUE_SIZE must > 0"
#endif

// ensure query operator will not get the writing block.
#if (TIME_LIMIT_QUEUE_BACKUP_SIZE <= 0)
#error "TIME_LIMIT_QUEUE_BACKUP_SIZE must > 0"
#endif

#define CHECK_FLEETINGQUEUE_USAGE_READ(x) \
  (x & IFDContainer::eFDContainer_Opt_Read)
#define CHECK_FLEETINGQUEUE_USAGE_WRITE(x) \
  (x & IFDContainer::eFDContainer_Opt_Write)

// ----------------------------------------------------------------------------
// factory
// ----------------------------------------------------------------------------
std::shared_ptr<IFDContainer> IFDContainer::createInstance(
    char const* userId, IFDContainer::eFDContainer_Opt opt) {
  return std::make_shared<FDContainer>(userId, opt);
}

// ----------------------------------------------------------------------------
// FD info queue, singleton, thread-safe
// ----------------------------------------------------------------------------
static NSCam::FleetingQueue<FD_DATATYPE,
                            TIME_LIMIT_QUEUE_SIZE,
                            TIME_LIMIT_QUEUE_BACKUP_SIZE>
    sFleetingQueue;
class NSCam::FDContainerImp {
  // method
 public:
  std::vector<FD_DATATYPE*> getInfo(char const* userId) {
    return sFleetingQueue.getInfo(userId);
  }

  std::vector<FD_DATATYPE*> getInfo(char const* userId,
                                    const int64_t& ts_start,
                                    const int64_t& ts_end) {
    return sFleetingQueue.getInfo(userId, ts_start, ts_end);
  }

  std::vector<FD_DATATYPE*> getInfo(char const* userId,
                                    const std::vector<int64_t>& vecTss) {
    return sFleetingQueue.getInfo(userId, vecTss);
  }

  MBOOL returnInfo(char const* userId,
                   const std::vector<FD_DATATYPE*>& vecInfos) {
    return sFleetingQueue.returnInfo(userId, vecInfos);
  }

  FD_DATATYPE* editInfo(char const* userId, int64_t timestamp) {
    return sFleetingQueue.editInfo(userId, timestamp);
  }

  MBOOL publishInfo(char const* userId, FD_DATATYPE* info) {
    return sFleetingQueue.publishInfo(userId, info);
  }

  void dumpInfo() { sFleetingQueue.dumpInfo(); }

 public:
  static std::shared_ptr<FDContainerImp> getInstance() {
    std::lock_guard<std::mutex> __l(sLock);
    // create instance
    std::shared_ptr<FDContainerImp> inst = sInstance.lock();
    if (inst.get() == nullptr) {
      inst = std::shared_ptr<FDContainerImp>(new FDContainerImp);
      sInstance = inst;
    }
    return inst;
  }

  static bool hasInstance() {
    std::lock_guard<std::mutex> __l(sLock);

    return sInstance.use_count();
  }

 public:
  FDContainerImp() {}

  virtual ~FDContainerImp() {}

 private:
  // singleton of weak pointer implementation
  static std::mutex sLock;
  static std::weak_ptr<FDContainerImp> sInstance;
};

using NSCam::FDContainerImp;

std::mutex FDContainerImp::sLock;
std::weak_ptr<FDContainerImp> FDContainerImp::sInstance;

using NSCam::FDContainer;
// ----------------------------------------------------------------------------
// FDContainer
// ----------------------------------------------------------------------------
vector<FD_DATATYPE*> FDContainer::queryLock(void) {
  if (CHECK_FLEETINGQUEUE_USAGE_READ(mOpt) && mFleepingQueueImpl.get()) {
    return mFleepingQueueImpl->getInfo(mUserId);
  } else {
    CAM_LOGE("FDContainer instance is NULL");
  }

  CAM_LOGE("Not allow to query FDContainer");
  return vector<FD_DATATYPE*>();
}

vector<FD_DATATYPE*> FDContainer::queryLock(const int64_t& ts_start,
                                            const int64_t& ts_end) {
  if (CHECK_FLEETINGQUEUE_USAGE_READ(mOpt) && mFleepingQueueImpl.get()) {
    return mFleepingQueueImpl->getInfo(mUserId, ts_start, ts_end);
  } else {
    CAM_LOGE("FDContainer instance is NULL");
  }

  CAM_LOGE("Not allow to query FDContainer");
  return vector<FD_DATATYPE*>();
}

vector<FD_DATATYPE*> FDContainer::queryLock(const vector<int64_t>& vecTss) {
  if (CHECK_FLEETINGQUEUE_USAGE_READ(mOpt)) {
    if (mFleepingQueueImpl.get()) {
      return mFleepingQueueImpl->getInfo(mUserId, vecTss);
    }

    CAM_LOGE("FDContainer instance is NULL");
  }

  CAM_LOGE("Not allow to query FDContainer");
  return vector<FD_DATATYPE*>();
}

MBOOL FDContainer::queryUnlock(const vector<FD_DATATYPE*>& vecInfos) {
  if (CHECK_FLEETINGQUEUE_USAGE_READ(mOpt)) {
    if (mFleepingQueueImpl.get()) {
      return mFleepingQueueImpl->returnInfo(mUserId, vecInfos);
    }

    CAM_LOGE("FDContainer instance is NULL");
  }

  CAM_LOGE("Not allow to query FDContainer");
  return MFALSE;
}

FD_DATATYPE* FDContainer::editLock(int64_t timestamp) {
  if (CHECK_FLEETINGQUEUE_USAGE_WRITE(mOpt)) {
    if (mFleepingQueueImpl.get()) {
      return mFleepingQueueImpl->editInfo(mUserId, timestamp);
    }

    CAM_LOGE("FDContainer instance is NULL");
  }

  CAM_LOGE("Not allow to edit FDContainer");
  return nullptr;
}

MBOOL FDContainer::editUnlock(FD_DATATYPE* info) {
  if (CHECK_FLEETINGQUEUE_USAGE_WRITE(mOpt)) {
    if (mFleepingQueueImpl.get()) {
      return mFleepingQueueImpl->publishInfo(mUserId, info);
    }

    CAM_LOGE("FDContainer instance is NULL");
  }

  CAM_LOGE("Not allow to edit FDContainer");
  return MFALSE;
}

void FDContainer::dumpInfo() {
  if (mFleepingQueueImpl.get()) {
    mFleepingQueueImpl->dumpInfo();
  }
}

// ----------------------------------------------------------------------------
// constructor
// ----------------------------------------------------------------------------
FDContainer::FDContainer(char const* userId, IFDContainer::eFDContainer_Opt opt)
    : mUserId(userId), mOpt(opt) {
  mFleepingQueueImpl = FDContainerImp::getInstance();
}

FDContainer::~FDContainer() {
  mFleepingQueueImpl = nullptr;
}
