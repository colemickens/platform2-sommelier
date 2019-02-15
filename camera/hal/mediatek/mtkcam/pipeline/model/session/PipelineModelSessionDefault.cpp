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
#define LOG_TAG "mtkcam-PipelineModelSessionDefault"
// for scenario control
#include <mtkcam/drv/IHalSensor.h>
//
#include <impl/ControlMetaBufferGenerator.h>
#include <impl/PipelineContextBuilder.h>
#include <impl/PipelineFrameBuilder.h>
//
#include <memory>
#include <mtkcam/pipeline/hwnode/NodeId.h>
#include <mtkcam/pipeline/model/session/PipelineModelSessionDefault.h>
#include <mtkcam/pipeline/utils/streaminfo/IStreamInfoSetControl.h>
#include <string>
#include <vector>
#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/
using NSCam::v3::pipeline::model::PipelineModelSessionDefault;
/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::makeInstance(
    std::string const& name, CtorParams const& rCtorParams __unused)
    -> std::shared_ptr<IPipelineModelSession> {
  auto pSession =
      std::make_shared<PipelineModelSessionDefault>(name, rCtorParams);

  int const err = pSession->configure();
  if (CC_UNLIKELY(err != 0)) {
    CAM_LOGE("[%s] err:%d(%s) - Fail on configure()", __FUNCTION__, err,
             ::strerror(-err));
    return nullptr;
  }

  return pSession;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineModelSessionDefault::PipelineModelSessionDefault(
    std::string const& name, CtorParams const& rCtorParams)
    : PipelineModelSessionBase(
          {"Default/" +
           std::to_string(rCtorParams.staticInfo.pPipelineStaticInfo->openId)},
          rCtorParams) {
  pthread_rwlock_init(&mRWLock_ConfigInfo2, NULL);
  pthread_rwlock_init(&mRWLock_PipelineContext, NULL);
}

/******************************************************************************
 *
 ******************************************************************************/
namespace local {
template <typename T>
static std::string toString(const std::vector<T>& o) {
  std::string os;
  for (size_t i = 0; i < o.size(); i++) {
    if (o.size() > 1) {
      os += base::StringPrintf(" [%zu] ", i);
    }
    os += toString(o[i]);
  }
  return os;
}
}  // namespace local

auto PipelineModelSessionDefault::print(ConfigInfo2 const& o __unused) {
  std::string os;
  os += "{ empty ,need implement }";
  return os;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::dumpState(
    const std::vector<std::string>& options __unused) -> void {
  PipelineModelSessionBase::dumpState(options);
  // ConfigInfo2
  std::shared_ptr<ConfigInfo2> pConfigInfo2;
  {
    pthread_rwlock_rdlock(&mRWLock_ConfigInfo2);
    pConfigInfo2 = mConfigInfo2;
    pthread_rwlock_unlock(&mRWLock_ConfigInfo2);
  }
  if (pConfigInfo2 != nullptr) {
    print(*pConfigInfo2);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::getCurrentPipelineContext() const
    -> std::shared_ptr<PipelineContext> {
  pthread_rwlock_rdlock(&mRWLock_PipelineContext);
  auto ret = mCurrentPipelineContext;
  pthread_rwlock_unlock(&mRWLock_PipelineContext);

  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::configure() -> int {
  // Allocate mConfigInfo2
  mConfigInfo2 = std::make_shared<ConfigInfo2>();

  // Initialize mConfigInfo2
  {
    NSCam::v3::pipeline::policy::pipelinesetting::ConfigurationOutputParams out{
        .pStreamingFeatureSetting = &mConfigInfo2->mStreamingFeatureSetting,
        .pCaptureFeatureSetting = &mConfigInfo2->mCaptureFeatureSetting,
        .pPipelineNodesNeed = &mConfigInfo2->mPipelineNodesNeed,
        .pSensorSetting = &mConfigInfo2->mvSensorSetting,
        .pP1HwSetting = &mConfigInfo2->mvP1HwSetting,
        .pP1DmaNeed = &mConfigInfo2->mvP1DmaNeed,
        .pParsedStreamInfo_P1 = &mConfigInfo2->mvParsedStreamInfo_P1,
        .pParsedStreamInfo_NonP1 = &mConfigInfo2->mParsedStreamInfo_NonP1,
        .pIsZSLMode = &mConfigInfo2->mIsZSLMode,
    };
    RETURN_ERROR_IF_NOT_OK(
        mPipelineSettingPolicy->evaluateConfiguration(&out, {}),
        "Fail on evaluateConfiguration");
  }

  // App Image Max. Buffer Number
  RETURN_ERROR_IF_NOT_OK(
      mPipelineSettingPolicy->decideConfiguredAppImageStreamMaxBufNum(
          mStaticInfo.pUserConfiguration->pParsedAppImageStreamInfo.get(),
          &mConfigInfo2->mStreamingFeatureSetting,
          &mConfigInfo2->mCaptureFeatureSetting),
      "Fail on decideConfiguredAppImageStreamMaxBufNum");

  // some feature needs some information which get from config policy update.
  // And, it has to do related before build pipeline context.
  // This interface will help to do this.
  RETURN_ERROR_IF_NOT_OK(updateBeforeBuildPipelineContext(),
                         "updateBeforeBuildPipelineContext faile");
  // create capture related instances, MUST be after FeatureSettingPolicy
  configureCaptureInFlight(
      mConfigInfo2->mCaptureFeatureSetting.maxAppJpegStreamNum);

  // build pipeline context
  {
    BuildPipelineContextInputParams const in{
        .pipelineName = getSessionName(),
        .pPipelineStaticInfo = mStaticInfo.pPipelineStaticInfo.get(),
        .pPipelineUserConfiguration = mStaticInfo.pUserConfiguration.get(),
        .pParsedStreamInfo_NonP1 = &mConfigInfo2->mParsedStreamInfo_NonP1,
        .pvParsedStreamInfo_P1 = &mConfigInfo2->mvParsedStreamInfo_P1,
        .pZSLProvider = nullptr,
        .pvSensorSetting = &mConfigInfo2->mvSensorSetting,
        .pvP1HwSetting = &mConfigInfo2->mvP1HwSetting,
        .pPipelineNodesNeed = &mConfigInfo2->mPipelineNodesNeed,
        .pStreamingFeatureSetting = &mConfigInfo2->mStreamingFeatureSetting,
        .pCaptureFeatureSetting = &mConfigInfo2->mCaptureFeatureSetting,
        .batchSize = 0,
        .pOldPipelineContext = nullptr,
        .pDataCallback = shared_from_this(),
        .bUsingMultiThreadToBuildPipelineContext =
            mbUsingMultiThreadToBuildPipelineContext,
        .bIsReconfigure = false,
    };

    RETURN_ERROR_IF_NOT_OK(buildPipelineContext(&mCurrentPipelineContext, in),
                           "Fail on buildPipelineContext");
  }

  ////////////////////////////////////////////////////////////////////////////

  // Initialize the current sensor settings.
  for (auto const& v : mConfigInfo2->mvSensorSetting) {
    mSensorMode.push_back(v.sensorMode);
    mSensorSize.push_back(v.sensorSize);
  }

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::updateBeforeBuildPipelineContext() -> int {
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::submitOneRequest(
    std::shared_ptr<ParsedAppRequest> const& request) -> int {
  // On this function, use this local variable to serve the request.
  std::shared_ptr<ConfigInfo2> pConfigInfo2;
  {
    pthread_rwlock_rdlock(&mRWLock_ConfigInfo2);
    pConfigInfo2 = mConfigInfo2;
    pthread_rwlock_unlock(&mRWLock_ConfigInfo2);
  }

  // Make a copy of the original App Meta Control
  IMetadata appControl;  // original app control
  {
    if (request == nullptr) {
      MY_LOGD("request is NULL");
      return -ENODEV;
    }
    auto pTempAppMetaControl =
        request->pAppMetaControlStreamBuffer->tryReadLock(LOG_TAG);
    if (CC_LIKELY(pTempAppMetaControl)) {
      appControl = *pTempAppMetaControl;
      request->pAppMetaControlStreamBuffer->unlock(LOG_TAG,
                                                   pTempAppMetaControl);
    }
  }

  // Evaluate a result for a request.
  NSCam::v3::pipeline::policy::pipelinesetting::RequestOutputParams out;
  {
    NSCam::v3::pipeline::policy::pipelinesetting::RequestInputParams const in{
        .requestNo = request->requestNo,
        .pRequest_AppImageStreamInfo = request->pParsedAppImageStreamInfo.get(),
        .pRequest_AppControl = &appControl,
        .pRequest_ParsedAppMetaControl = request->pParsedAppMetaControl.get(),
        .pConfiguration_PipelineNodesNeed = &pConfigInfo2->mPipelineNodesNeed,
        .pConfiguration_StreamInfo_NonP1 =
            &pConfigInfo2->mParsedStreamInfo_NonP1,
        .pConfiguration_StreamInfo_P1 = &pConfigInfo2->mvParsedStreamInfo_P1,
        .pSensorMode = &mSensorMode,
        .pSensorSize = &mSensorSize,
        .isZSLMode = pConfigInfo2->mIsZSLMode,
    };
    RETURN_ERROR_IF_NOT_OK(mPipelineSettingPolicy->evaluateRequest(&out, in),
                           "Fail on evaluateRequest - requestNo:%d",
                           in.requestNo);
  }

  // Reconfiguration Flow
  processReconfiguration(&out, &pConfigInfo2, request->requestNo);

  // PipelineContext
  auto pPipelineContext = getCurrentPipelineContext();
  RETURN_ERROR_IF_NULLPTR(pPipelineContext, -ENODEV, "Bad PipelineContext");

  ////////////////////////////////////////////////////////////////////////////
  // process each frame
  MUINT32 lastFrameNo = 0;
  auto processFrame = [&](NSCam::v3::pipeline::policy::pipelinesetting::
                              RequestResultParams const& result,
                          int frameType) -> int {
    std::vector<std::shared_ptr<IMetaStreamBuffer>> vAppMeta;
    generateControlAppMetaBuffer(
        &vAppMeta,
        (frameType == eFRAMETYPE_MAIN) ? request->pAppMetaControlStreamBuffer
                                       : nullptr,
        &appControl, result.additionalApp.get(),
        pConfigInfo2->mParsedStreamInfo_NonP1.pAppMeta_Control);

    std::vector<std::shared_ptr<HalMetaStreamBuffer>> vHalMeta;
    for (size_t i = 0; i < pConfigInfo2->mvParsedStreamInfo_P1.size(); ++i) {
      MY_LOGD("generate (%zu) in metadata", i);
      generateControlHalMetaBuffer(
          &vHalMeta, result.additionalHal[i].get(),
          pConfigInfo2->mvParsedStreamInfo_P1[i].pHalMeta_Control);
    }

    std::shared_ptr<IPipelineFrame> pPipelineFrame;
    BuildPipelineFrameInputParams const params = {
        .requestNo = request->requestNo,
        .pAppImageStreamBuffers =
            (frameType == eFRAMETYPE_MAIN
                 ? request->pParsedAppImageStreamBuffers.get()
                 : nullptr),
        .pAppMetaStreamBuffers = (vAppMeta.empty() ? nullptr : &vAppMeta),
        .pHalImageStreamBuffers = nullptr,
        .pHalMetaStreamBuffers = (vHalMeta.empty() ? nullptr : &vHalMeta),
        .pvUpdatedImageStreamInfo = &(result.vUpdatedImageStreamInfo),
        .pnodeSet = &result.nodeSet,
        .pnodeIOMapImage = &(result.nodeIOMapImage),
        .pnodeIOMapMeta = &(result.nodeIOMapMeta),
        .pRootNodes = &(result.roots),
        .pEdges = &(result.edges),
        .pCallback =
            (frameType == eFRAMETYPE_MAIN ? shared_from_this() : nullptr),
        .pPipelineContext = pPipelineContext};

    MY_LOGD(
        "process request (%u): frametype(%d), sub(%u), preDummy(%u), "
        "postDummy(%u), enableZSL(%d)",
        request->requestNo, frameType, out.subFrames.size(),
        out.preDummyFrames.size(), out.postDummyFrames.size(), out.needZslFlow);

    RETURN_ERROR_IF_NOT_OK(buildPipelineFrame(&pPipelineFrame, params),
                           "buildPipelineFrame fail - requestNo:%u",
                           request->requestNo);
    lastFrameNo = pPipelineFrame->getFrameNo();
    RETURN_ERROR_IF_NOT_OK(pPipelineContext->queue(pPipelineFrame),
                           "PipelineContext::queue fail - requestNo:%u",
                           request->requestNo);

    return OK;
  };

  // pre-dummy frames
  for (auto const& frame : out.preDummyFrames) {
    RETURN_ERROR_IF_NOT_OK(processFrame(*frame, eFRAMETYPE_PREDUMMY),
                           "processFrame preDummyFrame fail");
  }
  // main frame
  {
    RETURN_ERROR_IF_NOT_OK(processFrame(*out.mainFrame, eFRAMETYPE_MAIN),
                           "processFrame mainFrame fail");
  }
  // sub frames
  for (auto const& frame : out.subFrames) {
    RETURN_ERROR_IF_NOT_OK(processFrame(*frame, eFRAMETYPE_SUB),
                           "processFrame subFrame fail");
  }
  // post-dummy frames
  for (auto const& frame : out.postDummyFrames) {
    RETURN_ERROR_IF_NOT_OK(processFrame(*frame, eFRAMETYPE_POSTDUMMY),
                           "processFrame postDummyFrame fail");
  }

  if (out.mainFrame->nodesNeed.needJpegNode) {
    mpCaptureInFlightRequest->insertRequest(request->requestNo,
                                            eMSG_INFLIGHT_NORMAL);
  }

  if (out.boostScenario != -1 &&
      out.boostScenario != IScenarioControlV3::Scenario_None) {
    mpScenarioCtrl->boostScenario(out.boostScenario, out.featureFlag,
                                  lastFrameNo);
  }

  // End of Reconfiguration Flow
  processEndSubmitOneRequest(&out);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::beginFlush() -> int {
  auto pPipelineContext = getCurrentPipelineContext();
  RETURN_ERROR_IF_NULLPTR(pPipelineContext, OK, "No current pipeline context");
  RETURN_ERROR_IF_NOT_OK(pPipelineContext->flush(), "PipelineContext::flush()");
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineModelSessionDefault::updateFrame(MUINT32 const requestNo,
                                         MINTPTR const userId,
                                         Result const& result) {
  if (result.bFrameEnd) {
    mpCaptureInFlightRequest->removeRequest(requestNo);
    return;
  }

  StreamId_T streamId = -1L;
  {
    pthread_rwlock_rdlock(&mRWLock_ConfigInfo2);
    streamId = mConfigInfo2->mvParsedStreamInfo_P1[0]
                   .pHalMeta_DynamicP1->getStreamId();
    pthread_rwlock_unlock(&mRWLock_ConfigInfo2);
  }
  auto timestampStartOfFrame =
      determineTimestampSOF(streamId, result.vHalOutMeta);
  updateFrameTimestamp(requestNo, userId, result, timestampStartOfFrame);
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::updateFrameTimestamp(
    MUINT32 const requestNo,
    MINTPTR const userId,
    Result const& result,
    int64_t timestampStartOfFrame) -> void {
  PipelineModelSessionBase::updateFrameTimestamp(requestNo, userId, result,
                                                 timestampStartOfFrame);
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::processReconfiguration(
    NSCam::v3::pipeline::policy::pipelinesetting::RequestOutputParams*
        rcfOutputParam,
    std::shared_ptr<ConfigInfo2>* pConfigInfo2 __unused,
    MUINT32 requestNo __unused) -> int {
  if (!rcfOutputParam->needReconfiguration) {
    return OK;
  }
  MY_LOGW("[TODO] needReconfiguration - Not Implement");
  return BAD_VALUE;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::configureCaptureInFlight(const int maxJpegNum)
    -> int {
  mpCaptureInFlightRequest = ICaptureInFlightRequest::createInstance(
      mStaticInfo.pPipelineStaticInfo->openId, mSessionName);
  if (!mpCaptureInFlightRequest) {
    MY_LOGE("fail to create CaptureInFlighRequest");
    return BAD_VALUE;
  }
  INextCaptureListener::CtorParams ctorParams;
  ctorParams.maxJpegNum = maxJpegNum;
  ctorParams.pCallback = mPipelineModelCallback;

  mpNextCaptureListener = INextCaptureListener::createInstance(
      mStaticInfo.pPipelineStaticInfo->openId, mSessionName, ctorParams);
  if (mpNextCaptureListener) {
    mpCaptureInFlightRequest->registerListener(mpNextCaptureListener);
  } else {
    MY_LOGE("fail to create NextCaptureListener");
    return BAD_VALUE;
  }

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionDefault::processEndSubmitOneRequest(
    NSCam::v3::pipeline::policy::pipelinesetting::RequestOutputParams*
        rcfOutputParam __unused) -> int {
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineModelSessionDefault::onNextCaptureCallBack(MUINT32 requestNo,
                                                   MINTPTR nodeId __unused) {
  mpNextCaptureListener->onNextCaptureCallBack(requestNo);
}
