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
  P2BufferHandle(const std::shared_ptr<P2Request> pRequest,
                 ID_IMG id,
                 CaptureProcessor* pProcessor)
      : mpRequest(pRequest),
        mpP2Img(nullptr),
        mImgId(id),
        mpImageBuffer(nullptr),
        mpProcessor(pProcessor) {
    if (mpRequest->isValidImg(id)) {
      mpP2Img = mpRequest->getImg(id);
    }
  }

  P2BufferHandle(const std::shared_ptr<P2Request> pRequest,
                 std::shared_ptr<P2Img> pP2Img,
                 CaptureProcessor* pProcessor)
      : mpRequest(pRequest),
        mpP2Img(pP2Img),
        mImgId(OUT_YUV),
        mpImageBuffer(nullptr),
        mpProcessor(pProcessor) {}

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
    if (mpRequest != nullptr) {
      if (mImgId != OUT_YUV && mImgId != OUT_JPEG_YUV) {
        mpProcessor->releaseImage(mpRequest, mImgId);
      }
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
  CaptureProcessor* mpProcessor;
};

class P2MetadataHandle : public virtual NSCam::NSCamFeature::NSFeaturePipe::
                             NSCapture::MetadataHandle {
 public:
  P2MetadataHandle(const std::shared_ptr<P2Request> pRequest,
                   ID_META id,
                   CaptureProcessor* pProcessor)
      : mpRequest(pRequest),
        mpP2Meta(nullptr),
        mMetaId(id),
        mpMetadata(nullptr),
        mpProcessor(pProcessor) {}

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
    mpMetadata = nullptr;
    mpP2Meta = nullptr;
    if (mpRequest != nullptr) {
      mpProcessor->releaseMeta(mpRequest, mMetaId);
      mpRequest = nullptr;
    }
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
  CaptureProcessor* mpProcessor;
};

CaptureProcessor::CaptureProcessor() : Processor(P2_CAPTURE_THREAD_NAME) {
  MY_LOG_FUNC_ENTER();
  mbPass3AInfo =
      property_get_int32("vendor.debug.camera.p2c.pass3AInfo", 1) > 0;
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

  for (auto idx = 0; idx < MAX_OUT_META_COUNT; idx++) {
    if (mvMetaData[idx].mpMetadata != NULL) {
      delete mvMetaData[idx].mpMetadata;
      mvMetaData[idx].mpMetadata = NULL;
    }
  }

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
          metaId, std::make_shared<P2MetadataHandle>(pRequest, id, this));
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
      pCapRequest->addBuffer(
          bufId, std::make_shared<P2BufferHandle>(pRequest, id, this));
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

  MINT64 timestamp;
  std::shared_ptr<P2Meta> meta;
  // get timestamp from in-app if reprocessing or get from in-p1-app
  if (pRequest->isReprocess()) {
    if (pRequest->isValidMeta(IN_APP)) {
      meta = pRequest->getMeta(IN_APP);
      if (tryGet<MINT64>(meta, MTK_SENSOR_TIMESTAMP, &timestamp)) {
        MY_S_LOGD(log, "get ts from in-app %" PRId64, timestamp);
        pCapRequest->setTimeStamp(timestamp);
      } else {
        MY_S_LOGD(log, "Can't get ts from in-app!");
      }
    } else {
      MY_S_LOGD(log, "Can't get ts from in-app!");
    }
  } else {
    if (pRequest->isValidMeta(IN_P1_APP)) {
      meta = pRequest->getMeta(IN_P1_APP);
      if (tryGet<MINT64>(meta, MTK_SENSOR_TIMESTAMP, &timestamp)) {
        MY_S_LOGD(log, "get ts from in-p1-app %" PRId64, timestamp);
        pCapRequest->setTimeStamp(timestamp);
      } else {
        MY_S_LOGD(log, "Can't get ts from in-p1-app!");
      }
    }
  }

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

MVOID CaptureProcessor::releaseImage(std::shared_ptr<P2Request> pRequest,
                                     ID_IMG imgId) {
  TRACE_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:releaseImage()");
  std::lock_guard<std::mutex> _l(mPairLock);
  pRequest->releaseImg(imgId);
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID CaptureProcessor::releaseMeta(std::shared_ptr<P2Request> pRequest,
                                    ID_META metaId) {
  TRACE_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2_Capture:releaseMeta()");
  std::lock_guard<std::mutex> _l(mPairLock);
  if (metaId == OUT_HAL) {
    IMetadata* pMetadata = pRequest->getMeta(metaId)->getIMetadataPtr();
    auto it = mRequestPairs.begin();
    for (; it != mRequestPairs.end(); it++) {
      if (it->mNodeRequest == pRequest) {
        auto& pCapRequest = it->mPipeRequest;
        if (pCapRequest->getTimeStamp() < 0) {
          continue;
        }
        if (pRequest->isReprocess()) {
          // get from metadata queue and append 3A info to OUT_HAL
          MINT32 idx;
          for (idx = 0; idx < MAX_OUT_META_COUNT; idx++) {
            MY_S_LOGI(mLog, "idx %d, ts %" PRId64, idx,
                      mvMetaData[idx].mTimeStamp);
            if (mvMetaData[idx].mTimeStamp == pCapRequest->getTimeStamp()) {
              MUINT8 encodeType;
              MINT32 uniqueKey, requestNo, frameNo, fnumber;
              MINT32 focallength, exposureTime, iso, awbmode, focallength35mm;
              MINT32 lightsource, expProgram, sceneCapType, flashlightTime;
              MINT32 aeMeterMode, aeExpBias;
              IMetadata exif;
              IMetadata* pKeepMetaData = mvMetaData[idx].mpMetadata;
              encodeType = 0;
              uniqueKey = requestNo = frameNo = -1;

              if (tryGet<MUINT8>(pKeepMetaData, MTK_JPG_ENCODE_TYPE,
                                 &encodeType)) {
                trySet<MUINT8>(pMetadata, MTK_JPG_ENCODE_TYPE, encodeType);
              }
              if (tryGet<MINT32>(pKeepMetaData, MTK_PIPELINE_UNIQUE_KEY,
                                 &uniqueKey)) {
                trySet<MINT32>(pMetadata, MTK_PIPELINE_UNIQUE_KEY, uniqueKey);
              }
              if (tryGet<MINT32>(pKeepMetaData, MTK_PIPELINE_REQUEST_NUMBER,
                                 &requestNo)) {
                trySet<MINT32>(pMetadata, MTK_PIPELINE_REQUEST_NUMBER,
                               requestNo);
              }
              if (tryGet<MINT32>(pKeepMetaData, MTK_PIPELINE_FRAME_NUMBER,
                                 &frameNo)) {
                trySet<MINT32>(pMetadata, MTK_PIPELINE_FRAME_NUMBER, frameNo);
              }
              if (tryGet<IMetadata>(pKeepMetaData, MTK_3A_EXIF_METADATA,
                                    &exif)) {
                if (!mbPass3AInfo) {
#define extractExif(srcmeta, outmeta, tag, type, tmp) \
  do {                                                \
    if (tryGet<type>(&srcmeta, tag, &tmp)) {          \
      trySet<type>(&outmeta, tag, tmp);               \
    }                                                 \
  } while (0)

                  IMetadata metaExif;
                  extractExif(exif, metaExif, MTK_3A_EXIF_FNUMBER, MINT32,
                              fnumber);
                  extractExif(exif, metaExif, MTK_3A_EXIF_FOCAL_LENGTH, MINT32,
                              focallength);
                  extractExif(exif, metaExif, MTK_3A_EXIF_CAP_EXPOSURE_TIME,
                              MINT32, exposureTime);
                  extractExif(exif, metaExif, MTK_3A_EXIF_AE_ISO_SPEED, MINT32,
                              iso);
                  extractExif(exif, metaExif, MTK_3A_EXIF_FOCAL_LENGTH_35MM,
                              MINT32, focallength35mm);
                  extractExif(exif, metaExif, MTK_3A_EXIF_AWB_MODE, MINT32,
                              awbmode);
                  extractExif(exif, metaExif, MTK_3A_EXIF_LIGHT_SOURCE, MINT32,
                              lightsource);
                  extractExif(exif, metaExif, MTK_3A_EXIF_EXP_PROGRAM, MINT32,
                              expProgram);
                  extractExif(exif, metaExif, MTK_3A_EXIF_SCENE_CAP_TYPE,
                              MINT32, sceneCapType);
                  extractExif(exif, metaExif, MTK_3A_EXIF_FLASH_LIGHT_TIME_US,
                              MINT32, flashlightTime);
                  extractExif(exif, metaExif, MTK_3A_EXIF_AE_METER_MODE, MINT32,
                              aeMeterMode);
                  extractExif(exif, metaExif, MTK_3A_EXIF_AE_EXP_BIAS, MINT32,
                              aeExpBias);
                  trySet<IMetadata>(pMetadata, MTK_3A_EXIF_METADATA, metaExif);

#undef extractExif
                } else {
                  trySet<IMetadata>(pMetadata, MTK_3A_EXIF_METADATA, exif);
                }
              }
              MY_S_LOGI(mLog, "Set to OutMeta, e %d, u %d, req %d, frame %d",
                        encodeType, uniqueKey, requestNo, frameNo);
              break;
            }
          }
          if (idx == MAX_OUT_META_COUNT) {
            MY_S_LOGE(mLog, "Can't find OutMeta in Queue, ts %" PRId64,
                      pCapRequest->getTimeStamp());
          }
        } else {
          // save OUT_HAL to metadata queue
          mIdx = (mIdx + 1) % MAX_OUT_META_COUNT;
          if (mvMetaData[mIdx].mpMetadata != NULL) {
            delete mvMetaData[mIdx].mpMetadata;
          }
          mvMetaData[mIdx].mTimeStamp = pCapRequest->getTimeStamp();
          mvMetaData[mIdx].mpMetadata = new IMetadata();
          *mvMetaData[mIdx].mpMetadata = *pMetadata;
          IMetadata* pKeepMetaData = mvMetaData[mIdx].mpMetadata;
          MUINT8 encodeType;
          MINT32 uniqueKey, requestNo, frameNo;
          encodeType = 0;
          uniqueKey = requestNo = frameNo = -1;
          tryGet<MUINT8>(pKeepMetaData, MTK_JPG_ENCODE_TYPE, &encodeType);
          tryGet<MINT32>(pKeepMetaData, MTK_PIPELINE_UNIQUE_KEY, &uniqueKey);
          tryGet<MINT32>(pKeepMetaData, MTK_PIPELINE_REQUEST_NUMBER,
                         &requestNo);
          tryGet<MINT32>(pKeepMetaData, MTK_PIPELINE_FRAME_NUMBER, &frameNo);

          IMetadata exif;
          if (!tryGet<IMetadata>(pKeepMetaData, MTK_3A_EXIF_METADATA, &exif)) {
            MY_S_LOGW(mLog, "can't get exif metadata!");
          }
          MY_S_LOGI(mLog,
                    "add OutMeta to queue, ts %" PRId64
                    " e %d, u %d, req %d, frame %d",
                    mvMetaData[mIdx].mTimeStamp, encodeType, uniqueKey,
                    requestNo, frameNo);
        }
        break;
      }
    }
  }
  pRequest->releaseMeta(metaId);
  TRACE_S_FUNC_EXIT(mLog);
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
