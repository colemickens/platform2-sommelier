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

#include <cstring>
#include "camera/hal/mediatek/mtkcam/feature/featureCore/featurePipe/util/P2Operator.h"

#define PIPE_MODULE_TAG "vsdof_util"
#define PIPE_CLASS_TAG "P2Operator"
#define PIPE_LOG_TAG PIPE_MODULE_TAG PIPE_CLASS_TAG
#include <include/PipeLog.h>
#include <INormalStream.h>
#include <src/pass2/NormalStream.h>

#include <memory>

using NSImageio::NSIspio::EPortIndex_TUNING;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Instantiation.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
NSCam::NSCamFeature::NSFeaturePipe::P2Operator::P2Operator(
    const char* creatorName, MINT32 openId)
    : mCreatorName(creatorName), miOpenId(openId) {
  MY_LOGD("OpenId(%d) CreatorName(%s)", miOpenId, mCreatorName);
}

NSCam::NSCamFeature::NSFeaturePipe::P2Operator::~P2Operator() {
  MY_LOGD("deconstruction");
  onLastStrongRef(nullptr);
}

MVOID
NSCam::NSCamFeature::NSFeaturePipe::P2Operator::onLastStrongRef(
    const void* /*id*/) {
  if (mpINormalStream) {
    for (auto& it : mTuningBuffers) {
      it->unlockBuf("V4L2");
    }
    mpINormalStream->uninit(LOG_TAG);
  }
}
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Public Operations.
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
MBOOL
NSCam::NSCamFeature::NSFeaturePipe::P2Operator::configNormalStream(
    MINT32 tag, const StreamConfigure config) {
  MY_LOGI("configNormalStream+");
  mpINormalStream = std::make_shared<v4l2::NormalStream>(miOpenId);
  MBOOL ret = mpINormalStream->init(LOG_TAG, config,
                                    (NSCam::v4l2::ENormalStreamTag)tag);
  if (ret != MTRUE) {
    MY_LOGE("init failed");
    return MFALSE;
  }

  ret = mpINormalStream->requestBuffers(EPortIndex_TUNING,
                                        IImageBufferAllocator::ImgParam(0, 0),
                                        &mTuningBuffers);
  if (ret != MTRUE) {
    MY_LOGE("requestBuffers failed");
    return MFALSE;
  }

  return MTRUE;
}

MERROR
NSCam::NSCamFeature::NSFeaturePipe::P2Operator::enque(QParams* pEnqueParam,
                                                      const char* userName) {
  std::lock_guard<std::mutex> _l(mLock);

  if (!mpINormalStream) {
    MY_LOGE("normalstream nullptr");
    return UNKNOWN_ERROR;
  }

  if (!pEnqueParam->mpfnCallback || !pEnqueParam->mpfnEnQFailCallback) {
    MY_LOGE("P2Operator only support non-blocking p2 operations! (%p,%p)",
            pEnqueParam->mpfnCallback, pEnqueParam->mpfnEnQFailCallback);
    return UNKNOWN_ERROR;
  }

  MY_LOGD("enque [%s] +", userName);

  if (!mpINormalStream->enque(pEnqueParam)) {
    MY_LOGE("enque failed!");
    return MFALSE;
  }

  MY_LOGD("enque [%s] -", userName);

  return OK;
}

std::shared_ptr<IImageBuffer>
NSCam::NSCamFeature::NSFeaturePipe::P2Operator::getTuningBuffer() {
  std::unique_lock<std::mutex> _l(mLock);
  while (mTuningBuffers.empty()) {
    // blocking and wait buffer to enque (wait 1s and check again)
    auto status = mCond.wait_for(_l, std::chrono::seconds(1));
    if (status == std::cv_status::timeout) {
      MY_LOGE("Timeout wait for Tunning Buffer");
      return nullptr;
    }
  }
  auto buf = mTuningBuffers[0];
  mTuningBuffers.erase(mTuningBuffers.begin());
  return buf;
}

void NSCam::NSCamFeature::NSFeaturePipe::P2Operator::putTuningBuffer(
    std::shared_ptr<IImageBuffer> buf) {
  std::lock_guard<std::mutex> _l(mLock);
  mTuningBuffers.push_back(buf);
  mCond.notify_all();
}

MBOOL
NSCam::NSCamFeature::NSFeaturePipe::P2Operator::requsetCapBuffer(
    int type,
    MUINT32 width,
    MUINT32 height,
    MUINT32 format,
    MUINT32 number,
    std::vector<std::shared_ptr<IImageBuffer>>* p_buffers) {
  MY_LOGI("requsetCapBuffer+");

  MINT32 bufBoundaryInBytes[3] = {0, 0, 0};
  MUINT32 bufStridesInBytes[3] = {0, 0, 0};
  MUINT32 planeCount = NSCam::Utils::Format::queryPlaneCount(format);
  MUINT32 ret;

  for (unsigned i = 0; i < planeCount; ++i) {
    bufStridesInBytes[i] =
        (NSCam::Utils::Format::queryPlaneWidthInPixels(format, i, width) *
             NSCam::Utils::Format::queryPlaneBitsPerPixel(format, i) +
         7) /
        8;
  }

  IImageBufferAllocator::ImgParam imgParam(
      static_cast<EImageFormat>(format), MSize(width, height),
      bufStridesInBytes, bufBoundaryInBytes, static_cast<size_t>(planeCount));

  ret = mpINormalStream->requestBuffers(type, imgParam, p_buffers, number);
  if (ret != MTRUE) {
    MY_LOGE("requestBuffers failed");
    return MFALSE;
  }
  return MTRUE;
}

MERROR
NSCam::NSCamFeature::NSFeaturePipe::P2Operator::release() {
  return OK;
}
