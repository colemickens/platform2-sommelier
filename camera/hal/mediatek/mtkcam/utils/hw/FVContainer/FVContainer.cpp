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

#define LOG_TAG "MtkCam/HwUtils/FVContainer"

#include <cutils/compiler.h>

#include <FVContainer/FVContainer.h>

#include <map>
#include <memory>
#include <mtkcam/def/common.h>
#include <mtkcam/utils/hw/FleetingQueue.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/StlUtils.h>

#include <string>
#include <unordered_set>
#include <vector>

using NSCam::IFVContainer;

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
  (x & IFVContainer::eFVContainer_Opt_Read)
#define CHECK_FLEETINGQUEUE_USAGE_WRITE(x) \
  (x & IFVContainer::eFVContainer_Opt_Write)

// ----------------------------------------------------------------------------
// factory
// ----------------------------------------------------------------------------
std::shared_ptr<IFVContainer> IFVContainer::createInstance(
    char const* userId, IFVContainer::eFVContainer_Opt opt) {
  return std::make_shared<FVContainer>(userId, opt);
}

// ----------------------------------------------------------------------------
// Focus value queue, singleton, thread-safe
// ----------------------------------------------------------------------------
static NSCam::FleetingQueue<FV_DATATYPE,
                            TIME_LIMIT_QUEUE_SIZE,
                            TIME_LIMIT_QUEUE_BACKUP_SIZE>
    sFleetingQueue;
class NSCam::FVContainerImp {
  // method
 public:
  std::vector<FV_DATATYPE*> getInfo(char const* userId) {
    return sFleetingQueue.getInfo(userId);
  }

  std::vector<FV_DATATYPE*> getInfo(char const* userId,
                                    const int64_t& ts_start,
                                    const int64_t& ts_end) {
    return sFleetingQueue.getInfo(userId, ts_start, ts_end);
  }

  std::vector<FV_DATATYPE*> getInfo(char const* userId,
                                    const std::vector<int64_t>& vecTss) {
    return sFleetingQueue.getInfo(userId, vecTss);
  }

  MBOOL returnInfo(char const* userId,
                   const std::vector<FV_DATATYPE*>& vecInfos) {
    return sFleetingQueue.returnInfo(userId, vecInfos);
  }

  FV_DATATYPE* editInfo(char const* userId, int64_t timestamp) {
    return sFleetingQueue.editInfo(userId, timestamp);
  }

  MBOOL publishInfo(char const* userId, FV_DATATYPE* info) {
    return sFleetingQueue.publishInfo(userId, info);
  }

  void clear() { sFleetingQueue.clear(); }

  void dumpInfo() { sFleetingQueue.dumpInfo(); }

 public:
  static std::shared_ptr<FVContainerImp> getInstance() {
    std::lock_guard<std::mutex> __l(sLock);
    // create instance
    std::shared_ptr<FVContainerImp> inst = sInstance.lock();
    if (inst.get() == nullptr) {
      inst = std::shared_ptr<FVContainerImp>(new FVContainerImp);
      sInstance = inst;
    }
    return inst;
  }

  static bool hasInstance() {
    std::lock_guard<std::mutex> __l(sLock);

    return sInstance.use_count();
  }

 public:
  FVContainerImp() {}

  virtual ~FVContainerImp() {}

 private:
  // singleton of weak pointer implementation
  static std::mutex sLock;
  static std::weak_ptr<FVContainerImp> sInstance;
};

using NSCam::FVContainer;
using NSCam::FVContainerImp;

std::mutex FVContainerImp::sLock;
std::weak_ptr<FVContainerImp> FVContainerImp::sInstance;

// ----------------------------------------------------------------------------
// FVContainer
// ----------------------------------------------------------------------------
vector<FV_DATATYPE> FVContainer::query(void) {
  vector<FV_DATATYPE> ret;
  ret.clear();

  if (CHECK_FLEETINGQUEUE_USAGE_READ(mOpt)) {
    if (mFleepingQueueImpl.get()) {
      vector<FV_DATATYPE*> result = mFleepingQueueImpl->getInfo(mUserId);
      for (size_t i = 0; i < result.size(); i++) {
        if (CC_LIKELY(result[i] != nullptr)) {
          ret.push_back(*result[i]);
        }
      }
      mFleepingQueueImpl->returnInfo(mUserId, result);
      return ret;
    }

    CAM_LOGE("FVContainer instance is NULL");
  }

  CAM_LOGE("Not allow to query FVContainer");
  return ret;
}

vector<FV_DATATYPE> FVContainer::query(const int32_t& mg_start,
                                       const int32_t& mg_end) {
  vector<FV_DATATYPE> ret;
  ret.clear();

  if (CHECK_FLEETINGQUEUE_USAGE_READ(mOpt)) {
    if (mFleepingQueueImpl.get()) {
      vector<FV_DATATYPE*> result =
          mFleepingQueueImpl->getInfo(mUserId, static_cast<int64_t>(mg_start),
                                      static_cast<int64_t>(mg_end));
      for (size_t i = 0; i < result.size(); i++) {
        if (CC_LIKELY(result[i] != nullptr)) {
          ret.push_back(*result[i]);
        }
      }
      mFleepingQueueImpl->returnInfo(mUserId, result);
      return ret;
    }

    CAM_LOGE("FVContainer instance is NULL");
  }

  CAM_LOGE("Not allow to query FVContainer");
  return ret;
}

vector<FV_DATATYPE> FVContainer::query(const vector<int32_t>& vecMgs) {
  vector<FV_DATATYPE> ret;
  ret.clear();

  if (CHECK_FLEETINGQUEUE_USAGE_READ(mOpt)) {
    if (mFleepingQueueImpl.get()) {
      vector<int64_t> vecMgs64;
      vecMgs64.clear();
      for (size_t i = 0; i < vecMgs.size(); i++) {
        vecMgs64.push_back(static_cast<int64_t>(vecMgs[i]));
      }

      vector<FV_DATATYPE*> result =
          mFleepingQueueImpl->getInfo(mUserId, vecMgs64);
      for (size_t i = 0; i < result.size(); i++) {
        if (CC_LIKELY(result[i] != nullptr)) {
          ret.push_back(*result[i]);
        }
      }
      mFleepingQueueImpl->returnInfo(mUserId, result);
      return ret;
    }
    CAM_LOGE("FVContainer instance is NULL");
  }

  CAM_LOGE("Not allow to query FVContainer");
  return ret;
}

MBOOL FVContainer::push(int32_t magicNum, FV_DATATYPE fv) {
  if (CHECK_FLEETINGQUEUE_USAGE_WRITE(mOpt)) {
    if (mFleepingQueueImpl.get()) {
      FV_DATATYPE* editor =
          mFleepingQueueImpl->editInfo(mUserId, static_cast<int64_t>(magicNum));

      if (CC_LIKELY(editor != nullptr)) {
        *editor = fv;
        mFleepingQueueImpl->publishInfo(mUserId, editor);
        return MTRUE;
      }
      return MFALSE;
    }
    CAM_LOGE("FVContainer instance is NULL");
  }

  CAM_LOGE("Not allow to edit FVContainer");
  return MFALSE;
}

void FVContainer::clear() {
  if (mFleepingQueueImpl.get()) {
    mFleepingQueueImpl->clear();
  }
}

void FVContainer::dumpInfo() {
  if (mFleepingQueueImpl.get()) {
    mFleepingQueueImpl->dumpInfo();
  }
}

// ----------------------------------------------------------------------------
// constructor
// ----------------------------------------------------------------------------
FVContainer::FVContainer(char const* userId, IFVContainer::eFVContainer_Opt opt)
    : mUserId(userId), mOpt(opt) {
  mFleepingQueueImpl = FVContainerImp::getInstance();
}

FVContainer::~FVContainer() {
  mFleepingQueueImpl = nullptr;
}
