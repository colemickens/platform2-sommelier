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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_CaptureProcessor.h"

#define P2_CAPTURE_THREAD_NAME "p2_capture"

#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG CaptureProcessor
#define P2_TRACE TRACE_CAPTURE_PROCESSOR
#include "P2_LogHeader.h"
#include "P2_Param.h"

#include <memory>
#include <vector>

namespace P2 {

class P2BufferHandle : public virtual NSCam::NSCamFeature::NSFeaturePipe::
                           NSCapture::BufferHandle {
 public:
  P2BufferHandle(const std::shared_ptr<P2Request> pRequest, ID_IMG id)
      : mpRequest(pRequest),
        mpP2Img(nullptr),
        mImgId(id),
        mpImageBuffer(nullptr) {
    if (mpRequest->isValidImg(id)) {
      mpP2Img = mpRequest->getImg(id);
    }
  }

  P2BufferHandle(const std::shared_ptr<P2Request> pRequest,
                 std::shared_ptr<P2Img> pP2Img)
      : mpRequest(pRequest),
        mpP2Img(pP2Img),
        mImgId(OUT_YUV),
        mpImageBuffer(nullptr) {}

  virtual MERROR acquire(MINT usage) {
    (void)usage;

    if (mpP2Img == nullptr) {
      return BAD_VALUE;
    }

    mpP2Img->updateResult(MTRUE);
    mpImageBuffer = mpP2Img->getIImageBufferPtr();

    return OK;
  }

  virtual IImageBuffer* native() { return mpImageBuffer; }

  virtual void release() {
    if (mpRequest != nullptr && mImgId != OUT_YUV) {
      mpRequest->releaseImg(mImgId);
    }
    mpImageBuffer = nullptr;
    mpP2Img = nullptr;
    mpRequest = nullptr;
  }

  virtual MUINT32 getTransform() {
    if (mpP2Img == nullptr) {
      return 0;
    }
    return mpP2Img->getTransform();
  }

  virtual ~P2BufferHandle() {
    if (mpRequest != nullptr) {
      MY_LOGD("buffer(%d) not released", mImgId);
      release();
    }
  }

 private:
  std::shared_ptr<P2Request> mpRequest;
  std::shared_ptr<P2Img> mpP2Img;
  ID_IMG mImgId;
  IImageBuffer* mpImageBuffer;
};

class P2MetadataHandle : public virtual NSCam::NSCamFeature::NSFeaturePipe::
                             NSCapture::MetadataHandle {
 public:
  P2MetadataHandle(const std::shared_ptr<P2Request> pRequest, ID_META id)
      : mpRequest(pRequest),
        mpP2Meta(nullptr),
        mMetaId(id),
        mpMetadata(nullptr) {}

  virtual MERROR acquire() {
    if (mpRequest->isValidMeta(mMetaId)) {
      mpP2Meta = mpRequest->getMeta(mMetaId);
      mpP2Meta->updateResult(MTRUE);
      mpMetadata = mpP2Meta->getIMetadataPtr();
    } else {
      return BAD_VALUE;
    }
    return OK;
  }

  virtual IMetadata* native() { return mpMetadata; }

  virtual void release() {
    mpRequest->releaseMeta(mMetaId);
    mpMetadata = nullptr;
    mpP2Meta = nullptr;
    mpRequest = nullptr;
  }

  virtual ~P2MetadataHandle() {
    if (mpRequest != nullptr) {
      MY_LOGD("metadata(%d) not released", mMetaId);
      release();
    }
  }

 private:
  std::shared_ptr<P2Request> mpRequest;
  std::shared_ptr<P2Meta> mpP2Meta;
  ID_META mMetaId;
  IMetadata* mpMetadata;
};

CaptureProcessor::CaptureProcessor() : Processor(P2_CAPTURE_THREAD_NAME) {
  MY_LOG_FUNC_ENTER();
  MY_LOG_FUNC_EXIT();
}

CaptureProcessor::~CaptureProcessor() {
  MY_LOG_S_FUNC_ENTER(mLog);
  this->uninit();
  MY_LOG_S_FUNC_EXIT(mLog);
}

MBOOL CaptureProcessor::onInit(const P2InitParam& param) {
  ILog log = param.mP2Info.mLog;
  MY_LOG_S_FUNC_ENTER(log);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:init()");
  MBOOL ret = MTRUE;
  mP2Info = param.mP2Info;
  mLog = mP2Info.mLog;

  P2_CAM_TRACE_BEGIN(TRACE_DEFAULT, "P2_Capture:FeaturePipe create & init");

  const MINT32 sensorID = mP2Info.getConfigInfo().mMainSensorID;
  MY_LOGD("create captureFeaturePipe,sensorID:%d", sensorID);
  ICaptureFeaturePipe::UsageHint usage = ICaptureFeaturePipe::UsageHint();
  mpFeaturePipe = ICaptureFeaturePipe::createInstance(sensorID, usage);

  if (mpFeaturePipe == nullptr) {
    MY_S_LOGE(mLog, "OOM: cannot create FeaturePipe");
  } else {
    mpFeaturePipe->init();
  }

  mpCallback = std::make_shared<CaptureRequestCallback>(this);
  mpFeaturePipe->setCallback(mpCallback);

  P2_CAM_TRACE_END(TRACE_DEFAULT);
  MY_LOG_S_FUNC_EXIT(log);
  return ret;
}

MVOID CaptureProcessor::onUninit() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:uninit()");

  mpFeaturePipe->uninit();

  MY_LOG_S_FUNC_EXIT(mLog);
}

MVOID CaptureProcessor::onThreadStart() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:threadStart()");
  MY_LOG_S_FUNC_EXIT(mLog);
}

MVOID CaptureProcessor::onThreadStop() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:threadStop()");
  MY_LOG_S_FUNC_EXIT(mLog);
}

MBOOL CaptureProcessor::onConfig(const P2ConfigParam& param) {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:config()");
  mP2Info = param.mP2Info;
  mpFeaturePipe->config(mP2Info.getConfigInfo().mStreamConfigure);
  MY_LOG_S_FUNC_EXIT(mLog);
  return MTRUE;
}

MBOOL CaptureProcessor::onEnque(
    const std::shared_ptr<P2FrameRequest>& pP2Frame) {
  ILog log = spToILog(pP2Frame);
  TRACE_S_FUNC_ENTER(log);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:enque()");
  MBOOL ret = MTRUE;

  std::vector<std::shared_ptr<P2Request>> pP2Requests =
      pP2Frame->extractP2Requests();

  std::shared_ptr<P2Request> pRequest;
  for (auto& p : pP2Requests) {
    if (pRequest == nullptr) {
      pRequest = p;
    } else {
      MY_S_LOGW(log, "Not support multiple p2 requests");
      p->releaseResource(P2Request::RES_ALL);
    }
  }

  if (pRequest == nullptr) {
    MY_S_LOGW(log, "P2Request is NULL!");
    return MFALSE;
  }

  const P2FrameData& rFrameData = pRequest->mP2Pack.getFrameData();
  MUINT32 uMasterID = rFrameData.mMasterSensorID;

  if (uMasterID == INVALID_SENSOR_ID) {
    MY_S_LOGW(log, "Request masterId(%u). Skip frame.", uMasterID);
    return MFALSE;
  }

  std::shared_ptr<ICaptureFeatureRequest> pCapRequest =
      mpFeaturePipe->acquireRequest();
  pCapRequest->addParameter(
      NSCam::NSCamFeature::NSFeaturePipe::NSCapture::PID_REQUEST_NUM,
      rFrameData.mMWFrameRequestNo);
  pCapRequest->addParameter(
      NSCam::NSCamFeature::NSFeaturePipe::NSCapture::PID_FRAME_NUM,
      rFrameData.mMWFrameNo);

  MBOOL bBlendingFrame = MFALSE;
  MINT64 feature = 0;
  if (pRequest->isValidMeta(IN_P1_HAL)) {
    std::shared_ptr<P2Meta> meta = pRequest->getMeta(IN_P1_HAL);
    MINT32 count = 0;
    MINT32 index = 0;
    if (tryGet<MINT32>(meta, MTK_HAL_REQUEST_COUNT, &count) &&
        tryGet<MINT32>(meta, MTK_HAL_REQUEST_INDEX, &index)) {
      pCapRequest->addParameter(
          NSCam::NSCamFeature::NSFeaturePipe::NSCapture::PID_FRAME_COUNT,
          count);
      pCapRequest->addParameter(
          NSCam::NSCamFeature::NSFeaturePipe::NSCapture::PID_FRAME_INDEX,
          index);

      bBlendingFrame = index > 0;
    }

    if (tryGet<MINT64>(meta, MTK_FEATURE_CAPTURE, &feature)) {
      MY_LOGD("request count:%d index:%d feature:0x%" PRIx64, count, index,
              feature);

      if (feature & NSCam::NSPipelinePlugin::MTK_FEATURE_NR) {
        pCapRequest->addFeature(
            NSCam::NSCamFeature::NSFeaturePipe::NSCapture::FID_NR);
      }
    }
  }

  if (pRequest->isValidMeta(IN_APP)) {
    std::shared_ptr<P2Meta> meta = pRequest->getMeta(IN_APP);
    MINT32 trigger;
    if (tryGet<MINT32>(meta, MTK_CONTROL_CAPTURE_EARLY_NOTIFICATION_TRIGGER,
                       &trigger)) {
      trigger = trigger > 0 ? 1 : 0;
      pCapRequest->addParameter(NSCam::NSCamFeature::NSFeaturePipe::NSCapture::
                                    PID_ENABLE_NEXT_CAPTURE,
                                trigger);
    }
  }

  if (!bBlendingFrame && !pRequest->hasOutput()) {
    MY_S_LOGW(log, "Request has no output(%d)", pRequest->hasOutput());
    return MFALSE;
  }

  // Metadata
  auto MapMetadata =
      [&](ID_META id,
          NSCam::NSCamFeature::NSFeaturePipe::NSCapture::CaptureMetadataID
              metaId) -> void {
    if (pRequest->isValidMeta(id)) {
      pCapRequest->addMetadata(
          metaId, std::make_shared<P2MetadataHandle>(pRequest, id));
    }
  };

  MapMetadata(
      IN_P1_APP,
      NSCam::NSCamFeature::NSFeaturePipe::NSCapture::MID_MAN_IN_P1_DYNAMIC);
  MapMetadata(IN_P1_HAL,
              NSCam::NSCamFeature::NSFeaturePipe::NSCapture::MID_MAN_IN_HAL);
  MapMetadata(IN_APP,
              NSCam::NSCamFeature::NSFeaturePipe::NSCapture::MID_MAN_IN_APP);
  MapMetadata(OUT_APP,
              NSCam::NSCamFeature::NSFeaturePipe::NSCapture::MID_MAN_OUT_APP);
  MapMetadata(OUT_HAL,
              NSCam::NSCamFeature::NSFeaturePipe::NSCapture::MID_MAN_OUT_HAL);

  // Image
  auto MapBuffer =
      [&](ID_IMG id,
          NSCam::NSCamFeature::NSFeaturePipe::NSCapture::CaptureBufferID bufId)
      -> MBOOL {
    if (pRequest->isValidImg(id)) {
      pCapRequest->addBuffer(bufId,
                             std::make_shared<P2BufferHandle>(pRequest, id));
      return MTRUE;
    }
    return MFALSE;
  };

  MapBuffer(IN_OPAQUE,
            NSCam::NSCamFeature::NSFeaturePipe::NSCapture::BID_MAN_IN_FULL);
  MapBuffer(IN_FULL,
            NSCam::NSCamFeature::NSFeaturePipe::NSCapture::BID_MAN_IN_FULL);
  MapBuffer(IN_RESIZED,
            NSCam::NSCamFeature::NSFeaturePipe::NSCapture::BID_MAN_IN_RSZ);
  MapBuffer(IN_LCSO,
            NSCam::NSCamFeature::NSFeaturePipe::NSCapture::BID_MAN_IN_LCS);
  MapBuffer(IN_REPROCESS,
            NSCam::NSCamFeature::NSFeaturePipe::NSCapture::BID_MAN_IN_YUV);
  MapBuffer(OUT_JPEG_YUV,
            NSCam::NSCamFeature::NSFeaturePipe::NSCapture::BID_MAN_OUT_JPEG);
  MapBuffer(
      OUT_THN_YUV,
      NSCam::NSCamFeature::NSFeaturePipe::NSCapture::BID_MAN_OUT_THUMBNAIL);
  MapBuffer(
      OUT_POSTVIEW,
      NSCam::NSCamFeature::NSFeaturePipe::NSCapture::BID_MAN_OUT_POSTVIEW);

  size_t n = pRequest->mImgOutArray.size();
  if (n > 2) {
    MY_LOGW("can NOT support more than 2 yuv streams: %zu", n);
    n = 2;
  }
  for (size_t i = 0; i < n; i++) {
    pCapRequest->addBuffer(
        NSCam::NSCamFeature::NSFeaturePipe::NSCapture::BID_MAN_OUT_YUV00 + i,
        std::make_shared<P2BufferHandle>(pRequest, pRequest->mImgOutArray[i]));
  }
  // Trigger Batch Release
  pRequest->beginBatchRelease();

  // Request Pair
  {
    std::lock_guard<std::mutex> _l(mPairLock);
    mRequestPairs.emplace_back();
    auto& rPair = mRequestPairs.back();
    rPair.mPipeRequest = pCapRequest;
    rPair.mNodeRequest = pRequest;
  }

  MY_LOGD("enque request to captureFeaturePipe, req#:%d",
          pCapRequest->getRequestNo());
  mpFeaturePipe->enque(pCapRequest);

  MY_LOG_S_FUNC_EXIT(log);
  return ret;
}

MVOID CaptureProcessor::onNotifyFlush() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:notifyFlush()");
  if (mpFeaturePipe != nullptr) {
    mpFeaturePipe->flush();
  }
  MY_LOG_S_FUNC_EXIT(mLog);
}

MVOID CaptureProcessor::onWaitFlush() {
  MY_LOG_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:waitFlush()");
  MY_LOG_S_FUNC_EXIT(mLog);
}

CaptureRequestCallback::CaptureRequestCallback(CaptureProcessor* pProcessor)
    : mpProcessor(pProcessor) {}

void CaptureRequestCallback::onContinue(
    std::shared_ptr<ICaptureFeatureRequest> pCapRequest) {
  std::lock_guard<std::mutex> _l(mpProcessor->mPairLock);
  auto it = mpProcessor->mRequestPairs.begin();
  for (; it != mpProcessor->mRequestPairs.end(); it++) {
    if ((*it).mPipeRequest == pCapRequest) {
      std::shared_ptr<P2Request>& pRequest = (*it).mNodeRequest;
      pRequest->notifyNextCapture();
      break;
    }
  }
}

void CaptureRequestCallback::onAborted(
    std::shared_ptr<ICaptureFeatureRequest> pCapRequest) {
  std::lock_guard<std::mutex> _l(mpProcessor->mPairLock);
  auto it = mpProcessor->mRequestPairs.begin();
  for (; it != mpProcessor->mRequestPairs.end(); it++) {
    if ((*it).mPipeRequest == pCapRequest) {
      std::shared_ptr<P2Request>& pRequest = (*it).mNodeRequest;
      pRequest->updateResult(MFALSE);
      pRequest->updateMetaResult(MFALSE);
      pRequest->releaseResource(P2Request::RES_ALL);
      pRequest->endBatchRelease();
      mpProcessor->mRequestPairs.erase(it);
      break;
    }
  }
}

void CaptureRequestCallback::onCompleted(
    std::shared_ptr<ICaptureFeatureRequest> pCapRequest, MERROR ret) {
  std::lock_guard<std::mutex> _l(mpProcessor->mPairLock);
  auto it = mpProcessor->mRequestPairs.begin();
  for (; it != mpProcessor->mRequestPairs.end(); it++) {
    if ((*it).mPipeRequest == pCapRequest) {
      std::shared_ptr<P2Request>& pRequest = (*it).mNodeRequest;
      pRequest->updateResult(ret == OK);
      pRequest->updateMetaResult(ret == OK);
      pRequest->releaseResource(P2Request::RES_ALL);
      pRequest->endBatchRelease();
      mpProcessor->mRequestPairs.erase(it);
      break;
    }
  }
}

}  // namespace P2
