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

#include <common/include/DebugControl.h>

#define PIPE_CLASS_TAG "Pool"
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_NODE

#include <capture/buffer/CaptureBufferPool.h>
#include <common/include/PipeLog.h>
#include <INormalStream.h>
#include <memory>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

TuningBufferHandle::TuningBufferHandle(
    const std::shared_ptr<BufferPool<TuningBufferHandle> >& pool)
    : BufferHandle(pool), mpVA(0) {}

std::shared_ptr<TuningBufferPool> TuningBufferPool::create(const char* name,
                                                           MUINT32 size) {
  TRACE_FUNC_ENTER();
  std::shared_ptr<TuningBufferPool> pool =
      std::make_shared<TuningBufferPool>(name);
  if (pool == nullptr) {
    MY_LOGE("OOM: Cannot create TuningBufferPool!");
  } else if (!pool->init(size)) {
    MY_LOGE("Pool initialization failed!");
    pool = nullptr;
  }

  TRACE_FUNC_EXIT();
  return pool;
}

MVOID TuningBufferPool::destroy(std::shared_ptr<TuningBufferPool>* pPool) {
  TRACE_FUNC_ENTER();
  if (*pPool != nullptr) {
    (*pPool)->releaseAll();
    *pPool = nullptr;
  }
  TRACE_FUNC_EXIT();
}

TuningBufferPool::TuningBufferPool(const char* name)
    : BufferPool<TuningBufferHandle>(name), muBufSize(0) {}

TuningBufferPool::~TuningBufferPool() {
  uninit();
}

MBOOL TuningBufferPool::init(MUINT32 size) {
  TRACE_FUNC_ENTER();

  std::lock_guard<std::mutex> lock(mMutex);

  if (size <= 0) {
    return MFALSE;
  }

  muBufSize = size;

  TRACE_FUNC_EXIT();
  return MTRUE;
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
  std::shared_ptr<TuningBufferHandle> handle =
      std::make_shared<TuningBufferHandle>(shared_from_this());

  if (handle == nullptr) {
    MY_LOGE("TuningBufferHandle create failed!");
    return NULL;
  }

  handle->mpVA = reinterpret_cast<MVOID*>(malloc(muBufSize));

  if (handle->mpVA == NULL) {
    MY_LOGE("Out of memory!!");
    return NULL;
  }
  // clear memory
  memset(reinterpret_cast<void*>(handle->mpVA), 0, muBufSize);

  TRACE_FUNC_EXIT();

  return handle;
}

MBOOL TuningBufferPool::doRelease(std::shared_ptr<TuningBufferHandle> handle) {
  TRACE_FUNC_ENTER();

  if (handle->mpVA != NULL) {
    free(handle->mpVA);
    handle->mpVA = NULL;
  }

  TRACE_FUNC_EXIT();
  return MTRUE;
}

CaptureBufferPool::CaptureBufferPool(const char* name)
    : mName(name), mbInit(MFALSE), muTuningBufSize(0) {
  CAM_LOGD("name %s %d", mName, muTuningBufSize);
}

CaptureBufferPool::~CaptureBufferPool() {
  uninit();
}

MBOOL CaptureBufferPool::init(std::vector<BufferConfig> vBufConfig) {
  std::lock_guard<std::mutex> _l(mPoolLock);
  if (mbInit) {
    MY_LOGE("do init when it's already init!");
    return MFALSE;
  }

  mvImagePools.clear();

  for (auto& bufConf : vBufConfig) {
    MY_LOGD("[%s] s:%dx%d f:%d min:%d max:%d", bufConf.name, bufConf.width,
            bufConf.height, bufConf.format, bufConf.minCount, bufConf.maxCount);

    // SmartBuffer
    auto pImagePool = ImageBufferPool::create(
        bufConf.name, bufConf.width, bufConf.height,
        (EImageFormat)bufConf.format, ImageBufferPool::USAGE_HW_AND_SW, MFALSE);

    if (pImagePool == nullptr) {
      MY_LOGE("create [%s] failed!", bufConf.name);
      return MFALSE;
    }
    mvImagePools.emplace(
        std::make_tuple(bufConf.width, bufConf.height, bufConf.format),
        pImagePool);
  }
  mbInit = MTRUE;
  return MTRUE;
}

MBOOL CaptureBufferPool::uninit() {
  std::lock_guard<std::mutex> _l(mPoolLock);

  if (!mbInit) {
    MY_LOGE("do uninit when it's not init yet!");
    return MFALSE;
  }

  mvImagePools.clear();
  mbInit = MFALSE;
  return MTRUE;
}

MBOOL CaptureBufferPool::allocate() {
  return MTRUE;
}

MVOID CaptureBufferPool::addToPool(
    PoolKey_T poolKey, std::shared_ptr<ImageBufferPool> pImagePool) {
  mvImagePools.emplace(poolKey, pImagePool);
}

SmartImageBuffer CaptureBufferPool::getImageBuffer(MUINT32 width,
                                                   MUINT32 height,
                                                   MUINT32 format) {
  std::lock_guard<std::mutex> _l(mPoolLock);

  PoolKey_T poolKey = std::make_tuple(width, height, format);

  if (mvImagePools.find(poolKey) == mvImagePools.end()) {
    MBOOL bUseSingleBuffer = format == eImgFmt_I422;
    auto pImagePool = ImageBufferPool::create(
        "CapturePipe", width, height, (EImageFormat)format,
        ImageBufferPool::USAGE_HW_AND_SW, bUseSingleBuffer);
    if (pImagePool == nullptr) {
      MY_LOGE("create buffer pool failed!");
      std::shared_ptr<ImageBufferHandle> nullHandle(nullptr);
      return (SmartImageBuffer)nullHandle;
    }

    mvImagePools.emplace(poolKey, pImagePool);
  }

  std::shared_ptr<ImageBufferPool> pImagePool = mvImagePools.at(poolKey);
  MY_LOGD("poolsize = %d!", pImagePool->peakPoolSize());

  return pImagePool->request();
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
