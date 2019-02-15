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

#define LOG_TAG "mtkcam-ControlMetaBufferGenerator"

#include <impl/ControlMetaBufferGenerator.h>
//
#include "MyUtils.h"
//
#include <memory>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
// using namespace android;
using HalMetaStreamBuffer = NSCam::v3::Utils::HalMetaStreamBuffer;
using HalMetaStreamBufferAllocatorT = HalMetaStreamBuffer::Allocator;

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/******************************************************************************
 *
 ******************************************************************************/
auto generateControlAppMetaBuffer(
    std::vector<std::shared_ptr<IMetaStreamBuffer>>* out,
    std::shared_ptr<IMetaStreamBuffer> const pMetaStreamBuffer,
    IMetadata const* pOriginalAppControl,
    IMetadata const* pAdditionalApp,
    std::shared_ptr<const IMetaStreamInfo> pInfo) -> int {
  //  Append addition data to App Control IMetaStreamBuffer if it exists;
  //  otherwise allocate a new one with original + additional.

  std::shared_ptr<IMetaStreamBuffer> metaBuf;
  if (pMetaStreamBuffer != nullptr) {
    metaBuf = pMetaStreamBuffer;
    if (pAdditionalApp != nullptr) {
      IMetadata* appMeta = pMetaStreamBuffer->tryWriteLock(LOG_TAG);
      if (CC_UNLIKELY(appMeta == nullptr)) {
        MY_LOGE("Cannot get app control, somebody didn't release lock?");
        return -ENODEV;
      }
      *appMeta += *pAdditionalApp;
      pMetaStreamBuffer->unlock(LOG_TAG, appMeta);
    }
  } else {
    // sub/dummy frame app control : use original app control + additional
    IMetadata subMeta(*pOriginalAppControl);
    if (pAdditionalApp != nullptr) {
      subMeta += *pAdditionalApp;
    }
    metaBuf = HalMetaStreamBufferAllocatorT(
        std::const_pointer_cast<IMetaStreamInfo>(pInfo))(subMeta);
  }
  (*out).push_back(metaBuf);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto generateControlHalMetaBuffer(
    std::vector<std::shared_ptr<HalMetaStreamBuffer>>* out,
    IMetadata const* pAdditionalHal,
    std::shared_ptr<const IMetaStreamInfo> pInfo) -> int {
  if (pAdditionalHal != nullptr) {
    std::shared_ptr<HalMetaStreamBuffer> pBuffer =
        HalMetaStreamBufferAllocatorT(
            std::const_pointer_cast<IMetaStreamInfo>(pInfo))(*pAdditionalHal);
    (*out).push_back(pBuffer);
  }
  return OK;
}

};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
