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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_IMGBUFFERSTORE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_IMGBUFFERSTORE_H_

#include <featurePipe/common/include/BufferPool.h>
#include <map>
#include <memory>
#include "MtkHeader.h"
#include <mutex>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

class ImgBufferStore {
 public:
  ImgBufferStore();
  ~ImgBufferStore();

  MBOOL init(const std::shared_ptr<IBufferPool>& pool);
  MVOID uninit();
  IImageBuffer* requestBuffer();
  MBOOL returnBuffer(IImageBuffer* buffer);

 private:
  typedef std::map<IImageBuffer*, std::shared_ptr<IIBuffer> > RecordMap;
  std::mutex mPoolMutex;
  std::mutex mRecordMutex;
  std::shared_ptr<IBufferPool> mBufferPool;
  RecordMap mRecordMap;
};

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_IMGBUFFERSTORE_H_
