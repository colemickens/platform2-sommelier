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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERHANDLE_T_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERHANDLE_T_H_

#include "MtkHeader.h"
#include <atomic>
#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename T>
class BufferPool;

template <typename T>
class sb;

template <typename T>
class VISIBILITY_PUBLIC BufferHandle
    : public std::enable_shared_from_this<BufferHandle<T> > {
 public:
  explicit BufferHandle(const std::shared_ptr<BufferPool<T> >& pool);
  virtual ~BufferHandle();

 private:
  std::shared_ptr<BufferPool<T> > mPool;
  bool mTrack;
  std::atomic_int mCount;

  friend class BufferPool<T>;
  friend class sb<T>;
  void incSbCount();
  void decSbCount();
};

};      // namespace NSFeaturePipe
};      // namespace NSCamFeature
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERHANDLE_T_H_
