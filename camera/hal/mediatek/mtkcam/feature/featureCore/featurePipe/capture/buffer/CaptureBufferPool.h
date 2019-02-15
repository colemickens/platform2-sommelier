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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_BUFFER_CAPTUREBUFFERPOOL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_BUFFER_CAPTUREBUFFERPOOL_H_

#include "../CaptureFeature_Common.h"
#include "../CaptureFeatureNode.h"
#include <common/include/BufferPool.h>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

class TuningBufferPool;

class TuningBufferHandle
    : public NSFeaturePipe::BufferHandle<TuningBufferHandle> {
 public:
  TuningBufferHandle(
      const std::shared_ptr<BufferPool<TuningBufferHandle>>& pool);

 public:
  MVOID* mpVA;

 private:
  friend class TuningBufferPool;
};

typedef sb<TuningBufferHandle> SmartTuningBuffer;

class TuningBufferPool : public NSFeaturePipe::BufferPool<TuningBufferHandle> {
 public:
  static std::shared_ptr<TuningBufferPool> create(const char* name,
                                                  MUINT32 size);
  static MVOID destroy(std::shared_ptr<TuningBufferPool>* pPool);
  virtual ~TuningBufferPool();
  MUINT32 getBufSize() { return muBufSize; }

 public:
  explicit TuningBufferPool(const char* name);
  MBOOL init(MUINT32 size);
  MVOID uninit();
  virtual std::shared_ptr<TuningBufferHandle> doAllocate();
  virtual MBOOL doRelease(std::shared_ptr<TuningBufferHandle> handle);

 private:
  std::mutex mMutex;
  MUINT32 muBufSize;
};

struct BufferConfig {
  const char* name;
  MUINT32 width;
  MUINT32 height;
  EImageFormat format;
  MUINT32 usage;
  MUINT32 minCount;
  MUINT32 maxCount;
};

class CaptureBufferPool {
 public:
  explicit CaptureBufferPool(const char* name);
  virtual ~CaptureBufferPool();

  /**
   * @brief to setup buffer config.
   * @return
   *-true indicates ok; otherwise some error happened
   */
  MBOOL init(std::vector<BufferConfig> vBufConfig);

  /**
   * @brief to release buffer pools
   * @return
   *-true indicates ok; otherwise some error happened
   */
  MBOOL uninit();

  /**
   * @brief to do buffer allocation. Must call after init
   * @return
   *-true indicates the allocated buffer have not reach maximun yet; false
   *indicates it has finished all buffer allocation
   */
  MBOOL allocate();

  /**
   * @brief to get the bufPool
   * @return
   *-the pointer indicates success; nullptr indidcated failed
   */
  SmartImageBuffer getImageBuffer(MUINT32 width,
                                  MUINT32 height,
                                  MUINT32 format);

 private:
  typedef std::tuple<MUINT32, MUINT32, MUINT32> PoolKey_T;
  mutable std::mutex mPoolLock;
  const char* mName;
  MBOOL mbInit;
  MINT32 muTuningBufSize;

  // Image buffers
  std::map<PoolKey_T, std::shared_ptr<ImageBufferPool>> mvImagePools;
};

/***************************************************************************
 * Namespace end.
 ***************************************************************************/
};      // namespace NSCapture
};      // namespace NSFeaturePipe
};      // namespace NSCamFeature
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_BUFFER_CAPTUREBUFFERPOOL_H_
