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

#include "DebugControl.h"
#define PIPE_CLASS_TAG "ImgBufferStore"
#define PIPE_TRACE TRACE_IMG_BUFFER_STORE
#include <featurePipe/common/include/PipeLog.h>
#include <featurePipe/streaming/ImgBufferStore.h>
#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

ImgBufferStore::ImgBufferStore() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

ImgBufferStore::~ImgBufferStore() {
  TRACE_FUNC_ENTER();
  uninit();
  TRACE_FUNC_EXIT();
}

MBOOL ImgBufferStore::init(const std::shared_ptr<IBufferPool>& pool) {
  TRACE_FUNC_ENTER();

  std::lock_guard<std::mutex> poolLock(mPoolMutex);
  std::lock_guard<std::mutex> recordLock(mRecordMutex);

  MBOOL ret = MFALSE;
  if (mBufferPool != nullptr) {
    MY_LOGE("Invalid init: init already called");
  } else if (pool == nullptr) {
    MY_LOGE("Invalid init: pool = NULL");
  } else {
    mBufferPool = pool;
    ret = MTRUE;
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MVOID ImgBufferStore::uninit() {
  TRACE_FUNC_ENTER();

  std::lock_guard<std::mutex> poolLock(mPoolMutex);
  std::lock_guard<std::mutex> recordLock(mRecordMutex);

  if (mRecordMap.size()) {
    MY_LOGE("Buffer(%zu) not returned before uninit, force free",
            mRecordMap.size());
    mRecordMap.clear();
  }
  mBufferPool = nullptr;
  TRACE_FUNC_EXIT();
}

IImageBuffer* ImgBufferStore::requestBuffer() {
  TRACE_FUNC_ENTER();
  IImageBuffer* buffer = nullptr;
  std::shared_ptr<IIBuffer> imgBuffer;

  mPoolMutex.lock();
  if (mBufferPool == nullptr) {
    MY_LOGE("Invalid state: pool is NULL");
  } else {
    imgBuffer = mBufferPool->requestIIBuffer();
    if (imgBuffer != nullptr) {
      buffer = imgBuffer->getImageBufferPtr();
    }
  }

  // 1. Lock record map mutex after requestIIBuffer()
  //    to prevent deadlock with returnBuffer()
  // 2. Lock record map mutex before unlock pool
  //    to prevent illegal uninit() called during requestBuffer()
  mRecordMutex.lock();
  mPoolMutex.unlock();

  if (imgBuffer == nullptr || buffer == nullptr) {
    MY_LOGE("Failed to get buffer: imgBuffer=%p", imgBuffer.get());
  } else {
    RecordMap::iterator it = mRecordMap.find(buffer);
    if (it != mRecordMap.end()) {
      MY_LOGE("Buffer(%p) already in record(size=%zu), old=%p, new=%p", buffer,
              mRecordMap.size(), it->second.get(), imgBuffer.get());
      buffer = nullptr;
    } else {
      mRecordMap[buffer] = imgBuffer;
    }
  }

  mRecordMutex.unlock();
  TRACE_FUNC_EXIT();
  return buffer;
}

MBOOL ImgBufferStore::returnBuffer(IImageBuffer* buffer) {
  TRACE_FUNC_ENTER();

  std::lock_guard<std::mutex> recordLock(mRecordMutex);

  MBOOL ret = MFALSE;
  if (!buffer) {
    MY_LOGE("Buffer is NULL");
  } else {
    RecordMap::iterator it = mRecordMap.find(buffer);
    if (it == mRecordMap.end()) {
      MY_LOGE("Cannot find buffer(%p) record(size=%zu)", buffer,
              mRecordMap.size());
    } else {
      mRecordMap.erase(it);
      ret = MTRUE;
    }
  }
  TRACE_FUNC_EXIT();
  return ret;
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
