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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IIBUFFER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IIBUFFER_H_

#include "MtkHeader.h"

#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class VISIBILITY_PUBLIC IIBuffer {
 public:
  IIBuffer();
  virtual ~IIBuffer();

  virtual std::shared_ptr<IImageBuffer> getImageBuffer() const = 0;
  virtual std::shared_ptr<IImageBuffer> operator->() const = 0;
  IImageBuffer* getImageBufferPtr() const;
  MBOOL syncCache(NSCam::eCacheCtrl ctrl);
};

class VISIBILITY_PUBLIC IIBuffer_IImageBuffer : public IIBuffer {
 public:
  explicit IIBuffer_IImageBuffer(IImageBuffer* buffer);
  virtual ~IIBuffer_IImageBuffer();
  virtual std::shared_ptr<IImageBuffer> getImageBuffer() const;
  virtual std::shared_ptr<IImageBuffer> operator->() const;

 private:
  std::shared_ptr<IImageBuffer> mBuffer;
};

};      // namespace NSFeaturePipe
};      // namespace NSCamFeature
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_IIBUFFER_H_
