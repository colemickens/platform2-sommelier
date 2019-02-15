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
#undef LOG_TAG
#define LOG_TAG "mtkcam-NextCaptureListener"
//
#include "MyUtils.h"
//
#include <impl/ResultUpdateHelper.h>
//
#include <memory>
#include <mtkcam/pipeline/hwnode/StreamId.h>
#include <mtkcam/pipeline/model/capture/NextCaptureListener.h>
#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <string>
//
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
//
/******************************************************************************
 *
 ******************************************************************************/

using NSCam::v3::pipeline::model::INextCaptureListener;
typedef NSCam::v3::Utils::HalMetaStreamBuffer::Allocator
    HalMetaStreamBufferAllocatorT;
/******************************************************************************
 *
 ******************************************************************************/
auto INextCaptureListener::createInstance(int32_t openId,
                                          std::string const& name,
                                          CtorParams const& rCtorParams)
    -> std::shared_ptr<INextCaptureListener> {
  auto pInstance =
      std::make_shared<NextCaptureListener>(openId, name, rCtorParams);

  return pInstance;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::pipeline::model::NextCaptureListener::NextCaptureListener(
    int32_t openId,
    std::string const& name,
    INextCaptureListener::CtorParams const& rCtorParams)
    : mOpenId(openId),
      mUserName(name),
      mMaxJpegNum(rCtorParams.maxJpegNum),
      mInFlightJpeg(0),
      mpPipelineModelCallback(rCtorParams.pCallback) {
  mpStreamInfo = std::make_shared<NSCam::v3::Utils::MetaStreamInfo>(
      "Meta:App:Callback", eSTREAMID_META_APP_DYNAMIC_CALLBACK,
      eSTREAMTYPE_META_OUT, 0);
}

/******************************************************************************
 *
 ******************************************************************************/
auto NSCam::v3::pipeline::model::NextCaptureListener::onCaptureInFlightUpdated(
    CaptureInFlightUpdated const& params) -> void {
  std::lock_guard<std::mutex> _l(mInFlightJpegRequestNoMutex);
  mInFlightJpeg = params.inFlightJpegCount;
  if (mlRequestNo.size() > 0) {
    size_t nextCaptureCnt = mMaxJpegNum - mInFlightJpeg;
    if (nextCaptureCnt > mlRequestNo.size()) {
      nextCaptureCnt = mlRequestNo.size();
    }
    auto item = mlRequestNo.begin();
    uint32_t requestNo;
    while (item != mlRequestNo.end() && nextCaptureCnt > 0) {
      requestNo = *item;
      onNextCaptureUpdated(requestNo);
      nextCaptureCnt--;
      item = mlRequestNo.erase(item);
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::pipeline::model::NextCaptureListener::onNextCaptureCallBack(
    MUINT32 requestNo) {
  std::lock_guard<std::mutex> _l(mInFlightJpegRequestNoMutex);
  if (mlRequestNo.size() > 0 || mInFlightJpeg >= mMaxJpegNum) {
    MY_LOGD("(in-flight, maxJpegNum) = (%d,%d), pending requestNo: %d",
            mInFlightJpeg, mMaxJpegNum, requestNo);
    mlRequestNo.push_back(requestNo);
  } else {
    onNextCaptureUpdated(requestNo);
  }
}

auto NSCam::v3::pipeline::model::NextCaptureListener::onNextCaptureUpdated(
    uint32_t requestNo) -> void {
  CAM_TRACE_NAME(__FUNCTION__);
  MY_LOGD("NextCapture requestNo %d", requestNo);
  std::shared_ptr<IPipelineModelCallback> pCallback;
  pCallback = mpPipelineModelCallback.lock();

  if (CC_UNLIKELY(!pCallback.get())) {
    MY_LOGE("can not promote pCallback for NextCapture");
    return;
  }

  // generate std::shared_ptr<IMetaStreamBuffer> with only
  // MTK_CONTROL_CAPTURE_NEXT_READY
  std::shared_ptr<NSCam::v3::Utils::HalMetaStreamBuffer>
      pNextCaptureMetaBuffer = HalMetaStreamBufferAllocatorT(mpStreamInfo)();

  IMetadata* meta = pNextCaptureMetaBuffer->tryWriteLock(LOG_TAG);
  IMetadata::setEntry<MINT32>(meta, MTK_CONTROL_CAPTURE_NEXT_READY, 1);
  pNextCaptureMetaBuffer->unlock(LOG_TAG, meta);
  pNextCaptureMetaBuffer->finishUserSetup();
  ResultUpdateHelper(mpPipelineModelCallback, requestNo, pNextCaptureMetaBuffer,
                     false);
}
