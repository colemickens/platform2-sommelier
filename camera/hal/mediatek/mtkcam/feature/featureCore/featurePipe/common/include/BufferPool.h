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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERPOOL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERPOOL_H_

#include "BufferPool_t.h"
#include "BufferHandle.h"

#include <algorithm>
#include <functional>
#include <memory>

#include "PipeLogHeaderBegin.h"
#include "DebugControl.h"
#define PIPE_TRACE TRACE_BUFFER_POOL
#define PIPE_CLASS_TAG "BufferPool"
#include "PipeLog.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename T>
BufferPool<T>::BufferPool(const char* name)
    : mName(name), mAutoFree(-1), mAutoAllocate(-1), mAllocatingCount(0) {}

template <typename T>
BufferPool<T>::~BufferPool() {}

template <typename T>
MUINT32 BufferPool<T>::preAllocate(MUINT32 count) {
  (void)count;

  return 0;
}

template <typename T>
MUINT32 BufferPool<T>::allocate() {
  MUINT32 result = 0;
  std::shared_ptr<T> handle = this->doAllocate();
  if (handle != nullptr) {
    this->addToPool(handle);
    result = 1;
  }

  return result;
}

template <typename T>
MUINT32 BufferPool<T>::allocate(MUINT32 count) {
  MUINT32 result, i;
  result = 0;
  this->preAllocate(count);
  for (i = 0; i < count; ++i) {
    std::shared_ptr<T> handle = this->doAllocate();
    if (handle != nullptr) {
      this->addToPool(handle);
      ++result;
    }
  }

  return result;
}

template <typename T>
sb<T> BufferPool<T>::request() {
  std::unique_lock<std::mutex> lck(mMutex);
  const std::shared_ptr<T> nullHandle(nullptr);
  sb<T> handle(nullHandle);

  do {
    if (!mAvailable.empty()) {
      handle = mAvailable.front();
      mAvailable.pop();
      break;
    } else if (mAutoAllocate > 0 &&
               ((mPool.size() + mAllocatingCount) < (MUINT32)mAutoAllocate)) {
      ++mAllocatingCount;
      mMutex.unlock();
      if (!this->allocate()) {
        MY_LOGE("%s: Auto allocate attemp failed", mName);
      }
      mMutex.lock();
      --mAllocatingCount;
    } else {
      mCondition.wait(lck);
    }
  } while (true);

  return handle;
}

template <typename T>
MUINT32 BufferPool<T>::peakPoolSize() const {
  std::lock_guard<std::mutex> lock(mMutex);

  return mPool.size();
}

template <typename T>
MUINT32 BufferPool<T>::peakAvailableSize() const {
  std::lock_guard<std::mutex> lock(mMutex);

  return mAvailable.size();
}

template <typename T>
MVOID BufferPool<T>::setAutoAllocate(MINT32 bound) {
  std::lock_guard<std::mutex> lock(mMutex);
  mAutoAllocate = bound;
}

template <typename T>
MVOID BufferPool<T>::setAutoFree(MINT32 bound) {
  std::lock_guard<std::mutex> lock(mMutex);
  mAutoFree = bound;
  autoFree_locked();
}

template <typename T>
EImageFormat BufferPool<T>::getImageFormat() const {
  return eImgFmt_UNKNOWN;
}

template <typename T>
MSize BufferPool<T>::getImageSize() const {
  return MSize(0, 0);
}

template <typename T>
std::shared_ptr<IIBuffer> BufferPool<T>::requestIIBuffer() {
  return nullptr;
}

template <typename T>
typename BufferPool<T>::CONTAINER_TYPE BufferPool<T>::getPoolContents() const {
  std::lock_guard<std::mutex> lock(mMutex);
  BufferPool<T>::CONTAINER_TYPE temp(mPool.begin(), mPool.end());

  return temp;
}

template <typename T>
MVOID BufferPool<T>::addToPool(const std::shared_ptr<T>& handle) {
  std::lock_guard<std::mutex> lock(mMutex);
  mPool.push_back(handle);
  mAvailable.push(handle);
  mCondition.notify_all();
}

template <typename T>
MVOID BufferPool<T>::releaseAll() {
  std::lock_guard<std::mutex> lock(mMutex);
  typename BufferPool<T>::POOL_TYPE::iterator it, end;

  if (mAvailable.size() != mPool.size()) {
    MY_LOGE("Some buffers are not released before pool released");
  }

  for (it = mPool.begin(), end = mPool.end(); it != end; ++it) {
    (*it)->mTrack = MFALSE;
  }

  while (!mAvailable.empty()) {
    this->doRelease(mAvailable.front());
    mAvailable.pop();
  }

  mPool.clear();
}

template <typename T>
MVOID BufferPool<T>::recycle(std::shared_ptr<T> handle) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (handle && handle->mTrack) {
    // TODO(mtk): check handle is from this pool
    if (mAutoFree >= 0 && mPool.size() > (unsigned)mAutoFree) {
      freeFromPool_locked(handle);
    } else {
      mAvailable.push(handle);
    }
    mCondition.notify_all();
  } else {
    this->doRelease(handle);
  }
}

template <typename T>
MBOOL BufferPool<T>::freeFromPool_locked(std::shared_ptr<T> handle) {
  // mMutex must be locked

  MBOOL ret = MFALSE;
  typename POOL_TYPE::iterator it;

  if (handle != nullptr) {
    it = std::find(mPool.begin(), mPool.end(), handle);
    if (it != mPool.end()) {
      mPool.erase(it);
      this->doRelease(handle);
      ret = MTRUE;
    }
  }

  return ret;
}

template <typename T>
MVOID BufferPool<T>::autoFree_locked() {
  // mMutex must be locked

  MUINT32 count;
  if (mAutoFree >= 0) {
    count = mPool.size();
    while ((count > (MUINT32)mAutoFree) && !mAvailable.empty()) {
      freeFromPool_locked(mAvailable.front());
      mAvailable.pop();
      --count;
    }
  }
}

template <typename T>
const char* BufferPool<T>::getName() const {
  return mName;
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#include "PipeLogHeaderEnd.h"
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERPOOL_H_
