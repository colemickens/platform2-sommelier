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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERHANDLE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERHANDLE_H_

#include "BufferHandle_t.h"
#include "BufferPool_t.h"
#include <atomic>
#include <memory>
#include <utility>
namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

template <typename T>
BufferHandle<T>::BufferHandle(const std::shared_ptr<BufferPool<T> >& pool)
    : mPool(pool), mTrack(true), mCount(0) {}

template <typename T>
BufferHandle<T>::~BufferHandle() {}

template <typename T>
void BufferHandle<T>::incSbCount() {
  int32_t old =
      std::atomic_fetch_add_explicit(&mCount, 1, std::memory_order_release);
  (void)old;
}

template <typename T>
void BufferHandle<T>::decSbCount() {
  int32_t old =
      std::atomic_fetch_sub_explicit(&mCount, 1, std::memory_order_release);
  if (old == 1) {
    std::shared_ptr<T> handle =
        std::dynamic_pointer_cast<T>(this->shared_from_this());
    mPool->recycle(handle);
  }
}

};  // namespace NSFeaturePipe
};  // namespace NSCamFeature
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_BUFFERHANDLE_H_
