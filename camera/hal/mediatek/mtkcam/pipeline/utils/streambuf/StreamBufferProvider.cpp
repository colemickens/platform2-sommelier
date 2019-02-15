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

#define LOG_TAG "MtkCam/StreamBufferProvider"
//
#include <memory>
#include <mtkcam/pipeline/utils/streambuf/StreamBufferProvider.h>
#include <mtkcam/utils/imgbuf/IGbmImageBufferHeap.h>
#include <mtkcam/utils/std/Log.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace Utils {

/******************************************************************************
 *
 ******************************************************************************/
HalImageStreamBufferProvider::HalImageStreamBufferProvider(
    std::shared_ptr<IImageStreamInfo> pStreamInfo,
    std::shared_ptr<IImageBufferHeap> pImageBufferHeap,
    std::weak_ptr<IStreamBufferProvider> pProvider)
    : HalImageStreamBuffer(
          pStreamInfo, std::weak_ptr<IStreamBufferPoolT>(), pImageBufferHeap),
      mpProvider(pProvider) {}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
HalImageStreamBufferProvider::releaseBuffer() {
  //
  auto pProvider = mpProvider.lock();
  //
  if (pProvider == nullptr) {
    MY_LOGW("[%s:%p] NULL promote of buffer pool:%p", ParentT::getName(), this,
            pProvider.get());
    return;
  }
  //
  MUINT32 bufstatus = getStatus();
  //  Reset buffer before returning to poolProvider
  resetBuffer();
  //  Release to Provider
  pProvider->enqueStreamBuffer(
      mStreamInfo,
      std::dynamic_pointer_cast<HalImageStreamBufferProvider>(
          shared_from_this()),
      bufstatus);
}

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace Utils
};  // namespace v3
};  // namespace NSCam
