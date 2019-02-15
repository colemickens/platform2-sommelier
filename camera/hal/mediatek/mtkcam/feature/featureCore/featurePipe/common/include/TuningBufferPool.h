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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_TUNINGBUFFERPOOL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_TUNINGBUFFERPOOL_H_

#include <memory>
#include <mutex>
#include "./BufferPool.h"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class TuningBufferPool;

enum TuningBufSyncCtrl {
  eTUNINGBUF_CACHECTRL_FLUSH,
  eTUNINGBUF_CACHECTRL_SYNC
};

class TuningBufferHandle : public BufferHandle<TuningBufferHandle> {
 public:
  TuningBufferHandle(
      const std::shared_ptr<BufferPool<TuningBufferHandle> >& pool);

 public:
  MVOID* mpVA;

 private:
  friend class TuningBufferPool;
};

typedef sb<TuningBufferHandle> SmartTuningBuffer;

class TuningBufferPool : public BufferPool<TuningBufferHandle> {
 public:
  static std::shared_ptr<TuningBufferPool> create(const char* name,
                                                  MUINT32 size,
                                                  MBOOL bufProtect = MFALSE);
  static MVOID destroy(std::shared_ptr<TuningBufferPool>* pool);
  virtual ~TuningBufferPool();
  MUINT32 getBufSize() { return miBufSize; }

 public:
  TuningBufferPool(const char* name, MBOOL bufProtect);
  MBOOL init(MUINT32 size);
  MVOID uninit();
  virtual std::shared_ptr<TuningBufferHandle> doAllocate();
  virtual MBOOL doRelease(std::shared_ptr<TuningBufferHandle> handle);

 private:
  std::mutex mMutex;
  MUINT32 miBufSize;
  MBOOL mIsBufProtect;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_TUNINGBUFFERPOOL_H_
