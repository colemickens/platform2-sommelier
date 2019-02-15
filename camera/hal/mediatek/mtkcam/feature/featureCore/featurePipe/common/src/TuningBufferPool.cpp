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

#include "../include/DebugControl.h"
#define PIPE_TRACE TRACE_TUNING_BUFFER_POOL
#define PIPE_CLASS_TAG "TuningBufferPool"

#include "../include/PipeLog.h"
#include "../include/TuningBufferPool.h"
#include <mtkcam/feature/utils/PostRedZone.h>
#include <property_lib.h>

#include <memory>
#include <utility>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

TuningBufferHandle::TuningBufferHandle(
    const std::shared_ptr<BufferPool<TuningBufferHandle> >& pool)
    : BufferHandle(pool), mpVA(0) {}

std::shared_ptr<TuningBufferPool> TuningBufferPool::create(const char* name,
                                                           MUINT32 size,
                                                           MBOOL bufProtect) {
  TRACE_FUNC_ENTER();
  MINT32 prop =
      property_get_int32(KEY_TUNING_BUF_POOL_PROTECT, VAL_TUNING_BUF_PROTECT);
  MBOOL useBufProtect = (prop != -1) ? (prop > 0) : bufProtect;
  std::shared_ptr<TuningBufferPool> pool =
      std::make_shared<TuningBufferPool>(name, useBufProtect);
  if (pool == nullptr) {
    MY_LOGE("OOM: Cannot create TuningBufferPool!");
  } else if (!pool->init(size)) {
    MY_LOGE("Pool initialization failed! size(%u)", size);
    pool = nullptr;
  } else {
    MY_LOGD("TuningBufPool(%s) created. size(%u), protect(%d)", name, size,
            useBufProtect);
  }
  TRACE_FUNC_EXIT();
  return pool;
}

MVOID TuningBufferPool::destroy(std::shared_ptr<TuningBufferPool>* pool) {
  TRACE_FUNC_ENTER();
  if (*pool != nullptr) {
    (*pool)->releaseAll();
    *pool = NULL;
  }
  TRACE_FUNC_EXIT();
}

TuningBufferPool::TuningBufferPool(const char* name, MBOOL bufProtect)
    : BufferPool<TuningBufferHandle>(name),
      miBufSize(0),
      mIsBufProtect(bufProtect)

{}

TuningBufferPool::~TuningBufferPool() {
  uninit();
}

MBOOL TuningBufferPool::init(MUINT32 size) {
  MBOOL ret = MTRUE;
  TRACE_FUNC_ENTER();

  std::lock_guard<std::mutex> lock(mMutex);

  if (size <= 0) {
    ret = MFALSE;
  }
  miBufSize = size;

  TRACE_FUNC_EXIT();
  return ret;
}

MVOID TuningBufferPool::uninit() {
  TRACE_FUNC_ENTER();
  std::lock_guard<std::mutex> lock(mMutex);
  this->releaseAll();
  TRACE_FUNC_EXIT();
}

std::shared_ptr<TuningBufferHandle> TuningBufferPool::doAllocate() {
  TRACE_FUNC_ENTER();

  std::lock_guard<std::mutex> lock(mMutex);
  std::shared_ptr<TuningBufferHandle> bufferHandle =
      std::make_shared<TuningBufferHandle>(shared_from_this());

  if (bufferHandle == nullptr) {
    MY_LOGE("TuningBufferHandle create failed!");
    return nullptr;
  }

  if (mIsBufProtect) {
    bufferHandle->mpVA = PostRedZone::mynew(miBufSize);
  } else {
    bufferHandle->mpVA = reinterpret_cast<MVOID*>(malloc(miBufSize));
  }

  if (bufferHandle->mpVA == NULL) {
    MY_LOGE("Out of memory!!");
    return NULL;
  }
  // clear memory
  memset(reinterpret_cast<void*>(bufferHandle->mpVA), 0, miBufSize);

  TRACE_FUNC_EXIT();

  return bufferHandle;
}

MBOOL TuningBufferPool::doRelease(std::shared_ptr<TuningBufferHandle> handle) {
  TRACE_FUNC_ENTER();

  if (mIsBufProtect) {
    PostRedZone::mydelete(handle->mpVA);
  } else {
    free(handle->mpVA);
  }

  TRACE_FUNC_EXIT();
  return MTRUE;
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
