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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERPROVIDER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERPROVIDER_H_
#include <mtkcam/def/common.h>
#include "IStreamBufferProvider.h"

#include <memory>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/
/**
 * An implementation of hal image stream buffer from provider with enaue/deque
 * ops.
 */
class HalImageStreamBufferProvider : public HalImageStreamBuffer {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                                Definitions.
 public:  ////                                Data.
  std::weak_ptr<IStreamBufferProvider> mpProvider;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                                Data Members.
 public:     ////                                Operations.
  HalImageStreamBufferProvider(
      std::shared_ptr<IImageStreamInfo> pStreamInfo,
      std::shared_ptr<IImageBufferHeap> pImageBufferHeap,
      std::weak_ptr<IStreamBufferProvider> pProvider);

 public:  ////                                Operations.
  virtual MVOID releaseBuffer();
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace Utils
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_UTILS_STREAMBUF_STREAMBUFFERPROVIDER_H_
