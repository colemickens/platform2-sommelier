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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERPOOL_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERPOOL_T_H_

#include "MtkHeader.h"

#include <condition_variable>
#include <mutex>

#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <queue>
#include <vector>

#include "BufferHandle.h"
#include "SmartBuffer.h"
#include "IIBuffer.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class VISIBILITY_PUBLIC IBufferPool {
 public:
  IBufferPool();
  virtual ~IBufferPool();
  virtual const char* getName() const = 0;
  virtual MUINT32 preAllocate(MUINT32 count) = 0;
  virtual MUINT32 allocate() = 0;
  virtual MUINT32 allocate(MUINT32 count) = 0;
  virtual MUINT32 peakPoolSize() const = 0;
  virtual MUINT32 peakAvailableSize() const = 0;
  virtual MVOID setAutoAllocate(MINT32 bound) = 0;
  virtual MVOID setAutoFree(MINT32 bound) = 0;

  virtual EImageFormat getImageFormat() const = 0;
  virtual MSize getImageSize() const = 0;
  virtual std::shared_ptr<IIBuffer> requestIIBuffer() = 0;

  static MVOID destroy(std::shared_ptr<IBufferPool>* pool);

 protected:
  virtual MVOID releaseAll() = 0;
};

template <typename T>
class VISIBILITY_PUBLIC BufferPool
    : public virtual IBufferPool,
      public std::enable_shared_from_this<BufferPool<T> > {
 public:
  typedef typename std::vector<std::shared_ptr<T> > CONTAINER_TYPE;

  explicit BufferPool(const char* mName);
  virtual ~BufferPool();
  virtual MUINT32 preAllocate(MUINT32 count);
  virtual MUINT32 allocate();
  virtual MUINT32 allocate(MUINT32 count);
  sb<T> request();
  virtual MUINT32 peakPoolSize() const;
  virtual MUINT32 peakAvailableSize() const;
  virtual MVOID setAutoAllocate(MINT32 bound);
  virtual MVOID setAutoFree(MINT32 bound);
  virtual EImageFormat getImageFormat() const;
  virtual MSize getImageSize() const;
  virtual std::shared_ptr<IIBuffer> requestIIBuffer();

  const char* getName() const;
  // Get pool internal, be careful
  CONTAINER_TYPE getPoolContents() const;

 protected:
  MVOID addToPool(const std::shared_ptr<T>& handle);
  virtual MVOID releaseAll();
  virtual std::shared_ptr<T> doAllocate() = 0;
  virtual MBOOL doRelease(std::shared_ptr<T> handle) = 0;

 private:
  friend class BufferHandle<T>;
  MVOID recycle(std::shared_ptr<T> handle);

  MBOOL freeFromPool_locked(std::shared_ptr<T> handle);
  MVOID autoFree_locked();

 protected:
  const char* mName;

 private:
  typedef typename std::list<std::shared_ptr<T> > POOL_TYPE;

  mutable std::mutex mMutex;
  std::condition_variable mCondition;
  POOL_TYPE mPool;
  std::queue<std::shared_ptr<T> > mAvailable;
  MINT32 mAutoFree;
  MINT32 mAutoAllocate;
  MINT32 mAllocatingCount;
};

};      // namespace NSFeaturePipe
};      // namespace NSCamFeature
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERPOOL_T_H_
