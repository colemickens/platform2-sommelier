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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_FLEETINGQUEUE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_FLEETINGQUEUE_H_

#ifndef LOG_TAG
#define LOG_TAG "MtkCam/HwUtils/FleetingQueue"
#endif
#define FLEETINGQUEUE "FleetingQueue"

// MTKCAM
#include <cutils/compiler.h>    // CC_LIKELY, CC_UNLIKELY
#include <mtkcam/def/common.h>  // MBOOL
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/StlUtils.h>  // NSCam::SpinLock

// STL
#include <cassert>  // assert
#include <inttypes.h>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

// enable additional debug info
// #define __DEBUG

#ifdef __DEBUG
#define FLEETINGQUEUE_FUNCTION_SCOPE                                         \
  auto __scope_logger__ = [](char const* f) -> std::shared_ptr<const char> { \
    CAM_LOGD("[%s] + ", f);                                                  \
    return std::shared_ptr<const char>(                                      \
        f, [](char const* p) { CAM_LOGD("[%s] -", p); });                    \
  }(__FUNCTION__)
#else
#define FLEETINGQUEUE_FUNCTION_SCOPE \
  do {                               \
  } while (0)
#endif

namespace NSCam {
template <class DATATYPE, size_t QUEUESIZE, size_t BACKUPSIZE>
class FleetingQueue {
 protected:
  class QIndex {
   public:
    size_t get() { return mCurIdx; }

    QIndex getHead() {
      return QIndex(
          (mCurIdx + static_cast<int>(mQuerySize / mMaxSize + 1) * mMaxSize -
           mQuerySize) %
              mMaxSize,
          mQuerySize, mMaxSize - mQuerySize);
    }

    void move() {
      if (CC_UNLIKELY(++mCurIdx == mMaxSize)) {
        mCurIdx = 0;
      }
    }

   public:
    QIndex() = delete;
    QIndex(size_t idx, size_t queue_size, size_t backup_size)
        : mCurIdx(idx),
          mQuerySize(queue_size),
          mMaxSize(queue_size + backup_size) {
      if (CC_UNLIKELY(mMaxSize == 0)) {
        CAM_LOGE("%s: queue_size + backup_size can not be zero.", __FUNCTION__);
      } else {
        idx %= mMaxSize;
      }
    }

   private:
    size_t mCurIdx;
    size_t mQuerySize;
    size_t mMaxSize;
  };

  class QMeta {
   public:
    MBOOL registerUserId(char const* userId) {
      if (CC_LIKELY(userId)) {
        vUserId.insert(std::string(userId));
        return MTRUE;
      }

      CAM_LOGE("%s: userId can not be NULL.", __FUNCTION__);
      return MFALSE;
    }

    MBOOL unregisterUserId(char const* userId) {
      if (CC_LIKELY(userId)) {
        auto it = vUserId.find(std::string(userId));
        if (CC_LIKELY(it != vUserId.end())) {
          vUserId.erase(it);
          return MTRUE;
        }

        CAM_LOGE("%s: userId(%s) can not be found in FleetingQueue.",
                 __FUNCTION__, userId);
        return MFALSE;
      }
      CAM_LOGE("%s: userId can not be NULL.", __FUNCTION__);
      return MFALSE;
    }

    size_t userCount() { return vUserId.size(); }

    void dumpInfo() {
      CAM_LOGD("%s: - %zu usage of pData(%p), isDirty(%d):", __FUNCTION__,
               userCount(), pData, isDirty);

      std::string usage;
      for (auto it = vUserId.begin(); it != vUserId.end(); it++) {
        usage += "[" + *it + "] ";
      }

      if (!usage.empty()) {
        CAM_LOGD("%s:   %s", __FUNCTION__, usage.c_str());
      }
    }

   public:
    QMeta() : timestamp(-1), pData(nullptr), isDirty(MTRUE) { vUserId.clear(); }

    explicit QMeta(DATATYPE* ptr) : timestamp(-1), pData(ptr), isDirty(MTRUE) {
      vUserId.clear();
    }

    ~QMeta() {
      if (pData) {
        delete (pData);
      }
    }

   public:
    int64_t timestamp;
    DATATYPE* pData;
    MBOOL isDirty;

   private:
    std::unordered_multiset<std::string> vUserId;
  };

 public:
  std::vector<DATATYPE*> getInfo(char const* userId) {
    std::lock_guard<NSCam::SpinLock> __l(mQueuelLock);

    std::vector<DATATYPE*> ret;

    for (QIndex idx = mQueueIndex.getHead(); idx.get() != mQueueIndex.get();
         idx.move()) {
      std::shared_ptr<QMeta> meta = mQueueMeta[idx.get()];
      if (CC_UNLIKELY(meta->isDirty == MTRUE)) {
        continue;
      }

      if (CC_LIKELY(meta->pData)) {
        if (meta->registerUserId(userId)) {
          ret.push_back(meta->pData);
        }
      } else {
        CAM_LOGW("%s: Queue pData is NULL", __FUNCTION__);
        meta->dumpInfo();
        return std::vector<DATATYPE*>();
      }
    }

    return ret;
  }

  std::vector<DATATYPE*> getInfo(char const* userId,
                                 const int64_t& ts_start,
                                 const int64_t& ts_end) {
    std::lock_guard<NSCam::SpinLock> __l(mQueuelLock);

    std::vector<DATATYPE*> ret;

    for (QIndex idx = mQueueIndex.getHead(); idx.get() != mQueueIndex.get();
         idx.move()) {
      std::shared_ptr<QMeta> meta = mQueueMeta[idx.get()];
      if (CC_UNLIKELY(meta->isDirty == MTRUE)) {
        continue;
      }

      if (CC_LIKELY(meta->pData)) {
        if (meta->timestamp >= ts_start && meta->timestamp <= ts_end) {
          if (meta->registerUserId(userId)) {
            ret.push_back(meta->pData);
          }
        }
      } else {
        CAM_LOGW("%s: Queue pData is NULL", __FUNCTION__);
        meta->dumpInfo();
        return std::vector<DATATYPE*>();
      }
    }

    return ret;
  }

  std::vector<DATATYPE*> getInfo(char const* userId,
                                 const std::vector<int64_t>& vecTss) {
    std::lock_guard<NSCam::SpinLock> __l(mQueuelLock);

    std::vector<DATATYPE*> ret;

    std::map<int64_t, std::shared_ptr<QMeta>> queryMap;
    queryMap.clear();

    // build queryMap
    for (QIndex idx = mQueueIndex.getHead(); idx.get() != mQueueIndex.get();
         idx.move()) {
      std::shared_ptr<QMeta> meta = mQueueMeta[idx.get()];
      if (CC_UNLIKELY(meta->isDirty == MTRUE)) {
        continue;
      }

      if (CC_LIKELY(meta->pData)) {
        queryMap[meta->timestamp] = meta;
      } else {
        CAM_LOGW("%s: Queue pData is NULL", __FUNCTION__);
        meta->dumpInfo();
        return std::vector<DATATYPE*>();
      }
    }

    // return found chunk in queryMap or nullptr
    for (size_t i = 0; i < vecTss.size(); i++) {
      std::shared_ptr<QMeta> meta = queryMap[vecTss[i]];

      // return nullptr if key is not found
      ret.push_back(nullptr);
      if (CC_LIKELY(meta.get())) {
        if (CC_LIKELY(meta->pData)) {
          if (CC_LIKELY(meta->timestamp == vecTss[i])) {
            if (meta->registerUserId(userId)) {
              ret.back() = meta->pData;
            }
          } else {
            CAM_LOGW("%s: Queue pData is non-sync", __FUNCTION__);
            meta->dumpInfo();
          }
        } else {
          CAM_LOGW("%s: Queue pData is NULL", __FUNCTION__);
          meta->dumpInfo();
        }
      }
    }

    return ret;
  }

  MBOOL returnInfo(char const* userId, const std::vector<DATATYPE*>& vecInfos) {
    std::lock_guard<NSCam::SpinLock> __l(mQueuelLock);
    std::map<DATATYPE*, std::shared_ptr<QMeta>> queryMap;
    queryMap.clear();

    // build queryMap
    for (size_t i = 0; i < mQueueMeta.size(); i++) {
      std::shared_ptr<QMeta> meta = mQueueMeta[i];
      if (CC_LIKELY(meta->pData)) {
        queryMap[meta->pData] = meta;
      } else {
        CAM_LOGW("%s: Queue pData is NULL", __FUNCTION__);
        meta->dumpInfo();
        return MFALSE;
      }
    }

    for (size_t i = 0; i < vecInfos.size(); i++) {
      if (CC_LIKELY(vecInfos[i])) {
        std::shared_ptr<QMeta> meta = queryMap[vecInfos[i]];

        if (CC_LIKELY(meta.get())) {
          if (CC_LIKELY(meta->pData)) {
            meta->unregisterUserId(userId);
          } else {
            CAM_LOGW("%s: Queue pData is NULL", __FUNCTION__);
            meta->dumpInfo();
            return MFALSE;
          }
        }
      }
    }
    return MTRUE;
  }

  DATATYPE* editInfo(char const* userId, int64_t timestamp) {
    std::lock_guard<NSCam::SpinLock> __l(mQueuelLock);

    if (mIsEditing == MTRUE) {
      CAM_LOGW(
          "%s: User(%s) is not allow write to FleetingQueue because it already "
          "under writing",
          __FUNCTION__, std::string(userId).c_str());
      return nullptr;
    }

    std::shared_ptr<QMeta> meta = mQueueMeta[mQueueIndex.get()];
    if (CC_LIKELY(meta->pData)) {
      if (meta->userCount() == 0) {
        if (meta->registerUserId(userId)) {
          meta->timestamp = timestamp;
          mIsEditing = MTRUE;
          // set the same timestamp to dirty
          for (size_t i = 0; i < mQueueMeta.size(); i++) {
            if (CC_LIKELY(mQueueMeta[i]->timestamp != timestamp)) {
              continue;
            }
            mQueueMeta[i]->isDirty = MTRUE;
          }
          return meta->pData;
        }
      } else {
        CAM_LOGE("%s: FleetingQueue is full", __FUNCTION__);
        meta->dumpInfo();
      }
    } else {
      CAM_LOGW("%s: Queue entry is NULL", __FUNCTION__);
      meta->dumpInfo();
    }

    return nullptr;
  }

  MBOOL publishInfo(char const* userId, DATATYPE* info) {
    std::lock_guard<NSCam::SpinLock> __l(mQueuelLock);

    if (mIsEditing == MFALSE) {
      CAM_LOGW("%s:  User(%s) is not in write state", __FUNCTION__,
               std::string(userId).c_str());
      return MFALSE;
    }

    std::shared_ptr<QMeta> meta = mQueueMeta[mQueueIndex.get()];
    if (CC_LIKELY(info)) {
      if (info == meta->pData) {
        if (meta->unregisterUserId(userId)) {
          mIsEditing = MFALSE;
          meta->isDirty = MFALSE;
          mQueueIndex.move();
          return MTRUE;
        }
        CAM_LOGE("%s: Someone else is editing FleetingQueue?", __FUNCTION__);
        meta->dumpInfo();
      } else {
        CAM_LOGW("%s: Published pData is not under writing", __FUNCTION__);
        meta->dumpInfo();
      }
    } else {
      CAM_LOGW("%s: Publish pData is NULL", __FUNCTION__);
    }

    return MFALSE;
  }

  void dumpInfo() {
    std::lock_guard<NSCam::SpinLock> __l(mQueuelLock);

    CAM_LOGD("%s: FleetingQueue mQueueIndex = %zu", __FUNCTION__,
             mQueueIndex.get());
    for (size_t i = 0; i < mQueueMeta.size(); i++) {
      std::shared_ptr<QMeta> meta = mQueueMeta[i];
      if (CC_LIKELY(meta->pData)) {
        CAM_LOGD("%s: FleetingQueue[%zu]->timestamp is %" PRIi64, __FUNCTION__,
                 i, meta->timestamp);
        meta->dumpInfo();
      } else {
        CAM_LOGD("%s: FleetingQueue[%zu] is null", __FUNCTION__, i);
      }
    }
  }

  void clear() {
    std::lock_guard<NSCam::SpinLock> __l(mQueuelLock);
    for (size_t i = 0; i < mQueueMeta.size(); i++) {
      mQueueMeta[i]->isDirty = MTRUE;
    }
  }

 public:
  FleetingQueue() : mQueueIndex(0, QUEUESIZE, BACKUPSIZE), mIsEditing(MFALSE) {
    FLEETINGQUEUE_FUNCTION_SCOPE;
    std::lock_guard<NSCam::SpinLock> __l(mQueuelLock);

    if (CC_UNLIKELY(QUEUESIZE == 0 || BACKUPSIZE == 0)) {
      CAM_LOGE("%s: Both QUEUESIZE and BACKUPSIZE can not be zero",
               __FUNCTION__);
      assert(0);
    }

    mQueueMeta.resize(QUEUESIZE + BACKUPSIZE, nullptr);

    // allocate time limit infos
    for (size_t i = 0; i < mQueueMeta.size(); i++) {
      mQueueMeta[i] = std::make_shared<QMeta>(new DATATYPE);
    }
  }

  virtual ~FleetingQueue() {
    FLEETINGQUEUE_FUNCTION_SCOPE;

    for (size_t i = 0; i < mQueueMeta.size(); i++) {
      if (CC_UNLIKELY(mQueueMeta[i]->userCount() != 0)) {
        CAM_LOGE("%s: FleetingQueue[%zu] is in-used", __FUNCTION__, i);
        mQueueMeta[i]->dumpInfo();
      }
    }
    mQueueMeta.clear();
  }

 private:
  // AttributesMeta;
  std::vector<std::shared_ptr<QMeta>> mQueueMeta;
  NSCam::SpinLock mQueuelLock;
  QIndex mQueueIndex;
  MBOOL mIsEditing;
};
}; /* namespace NSCam */

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_FLEETINGQUEUE_H_
