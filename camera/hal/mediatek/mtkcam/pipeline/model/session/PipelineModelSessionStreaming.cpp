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
#define LOG_TAG "mtkcam-PipelineModelSessionStreaming"
//
#include <memory>
#include <string>
// for scenario control
#include <mtkcam/drv/IHalSensor.h>
//
#include <impl/ControlMetaBufferGenerator.h>
#include <impl/PipelineContextBuilder.h>
#include <impl/PipelineFrameBuilder.h>
//
#include "MyUtils.h"
//
#include <mtkcam/pipeline/model/session/PipelineModelSessionStreaming.h>
#include <mtkcam/pipeline/hwnode/NodeId.h>
#include <mtkcam/pipeline/hwnode/P1Node.h>
#include <mtkcam/pipeline/hwnode/P2StreamingNode.h>
#include <mtkcam/pipeline/hwnode/JpegNode.h>
#include <mtkcam/utils/hw/HwInfoHelper.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
/******************************************************************************
 *
 ******************************************************************************/
using NSCam::v3::pipeline::model::PipelineModelSessionStreaming;

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionStreaming::makeInstance(std::string const& name,
                                                 CtorParams const& rCtorParams)
    -> std::shared_ptr<IPipelineModelSession> {
  std::shared_ptr<PipelineModelSessionStreaming> pSession =
      std::make_shared<PipelineModelSessionStreaming>(name, rCtorParams);
  if (CC_UNLIKELY(pSession == nullptr)) {
    CAM_LOGE("[%s] Bad pSession", __FUNCTION__);
    return nullptr;
  }

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
PipelineModelSessionStreaming::PipelineModelSessionStreaming(
    std::string const& name, CtorParams const& rCtorParams)
    : PipelineModelSessionDefault(
          {name +
           std::to_string(rCtorParams.staticInfo.pPipelineStaticInfo->openId)},
          rCtorParams) {
  mConfigInfo2 = nullptr;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineModelSessionStreaming::~PipelineModelSessionStreaming() {}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionStreaming::processReconfiguration(
    NSCam::v3::pipeline::policy::pipelinesetting::RequestOutputParams*
        rcfOutputParam __unused,
    std::shared_ptr<ConfigInfo2>* pConfigInfo2 __unused,
    MUINT32 requestNo) -> int {
  pthread_rwlock_wrlock(&mRWLock_PipelineContext);

  auto ret = BAD_VALUE;
  if (!rcfOutputParam->needReconfiguration) {
    ret = OK;
  } else if (rcfOutputParam->reconfigCategory ==
             NSCam::v3::pipeline::policy::ReCfgCtg::STREAMING) {
    // reconfigCategory is Stream
    if (processReconfigStream(rcfOutputParam, pConfigInfo2, requestNo) != OK) {
      MY_LOGE("reconfigCategory(%hhu): processReconfigStream Error",
              rcfOutputParam->reconfigCategory);
      ret = BAD_VALUE;
    }
    ret = OK;
  } else if (rcfOutputParam->reconfigCategory ==
             NSCam::v3::pipeline::policy::ReCfgCtg::CAPTURE) {
    ret = OK;
  }
  pthread_rwlock_unlock(&mRWLock_PipelineContext);

  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionStreaming::processReconfigStream(
    NSCam::v3::pipeline::policy::pipelinesetting::RequestOutputParams*
        rcfOutputParam __unused,
    std::shared_ptr<ConfigInfo2>* pConfigInfo2 __unused,
    MUINT32 requestNo) -> int {
  MY_LOGD("requestNo(%d) processReconfigStream +", requestNo);

  MBOOL ret = INVALID_OPERATION;
  ret = waitUntilP1NodeDrainedAndFlush();
  if (ret != OK) {
    ALOGE("waitUntilP1NodeDrainedAndFlush Fail!");
    return ret;
  }

  ret = waitUntilP2DrainedAndFlush();
  if (ret != OK) {
    ALOGE("waitUntilP2DrainedAndFlush Fail!");
    return ret;
  }
  mCurrentPipelineContext = nullptr;

  *pConfigInfo2 = std::make_shared<ConfigInfo2>();

  RETURN_ERROR_IF_NULLPTR(*pConfigInfo2, -ENODEV,
                          "Fail on make_shared<ConfigInfo2>");
  NSCam::v3::pipeline::policy::pipelinesetting::ConfigurationOutputParams
      rcfOutParam{
          .pStreamingFeatureSetting =
              &(*pConfigInfo2)->mStreamingFeatureSetting,
          .pCaptureFeatureSetting = &(*pConfigInfo2)->mCaptureFeatureSetting,
          .pPipelineNodesNeed = &(*pConfigInfo2)->mPipelineNodesNeed,
          .pSensorSetting = &(*pConfigInfo2)->mvSensorSetting,
          .pP1HwSetting = &(*pConfigInfo2)->mvP1HwSetting,
          .pP1DmaNeed = &(*pConfigInfo2)->mvP1DmaNeed,
          .pParsedStreamInfo_P1 = &(*pConfigInfo2)->mvParsedStreamInfo_P1,
          .pParsedStreamInfo_NonP1 = &(*pConfigInfo2)->mParsedStreamInfo_NonP1,
          .pIsZSLMode = &(*pConfigInfo2)->mIsZSLMode,
      };

  RETURN_ERROR_IF_NOT_OK(
      mPipelineSettingPolicy->evaluateConfiguration(&rcfOutParam, {}),
      "Fail on Pipeline Reconfiguration");

  // App Image Max. Buffer Number
  RETURN_ERROR_IF_NOT_OK(
      mPipelineSettingPolicy->decideConfiguredAppImageStreamMaxBufNum(
          mStaticInfo.pUserConfiguration->pParsedAppImageStreamInfo.get(),
          &(*pConfigInfo2)->mStreamingFeatureSetting,
          &(*pConfigInfo2)->mCaptureFeatureSetting),
      "Fail on decideConfiguredAppImageStreamMaxBufNum");

  // create capture related instances, MUST be after FeatureSettingPolicy
  configureCaptureInFlight(
      (*pConfigInfo2)->mCaptureFeatureSetting.maxAppJpegStreamNum);

  BuildPipelineContextInputParams const in{
      .pipelineName = getSessionName(),
      .pPipelineStaticInfo = mStaticInfo.pPipelineStaticInfo.get(),
      .pPipelineUserConfiguration = mStaticInfo.pUserConfiguration.get(),
      .pParsedStreamInfo_NonP1 = &(*pConfigInfo2)->mParsedStreamInfo_NonP1,
      .pvParsedStreamInfo_P1 = &(*pConfigInfo2)->mvParsedStreamInfo_P1,
      .pZSLProvider = nullptr,
      .pvSensorSetting = &(*pConfigInfo2)->mvSensorSetting,
      .pvP1HwSetting = &(*pConfigInfo2)->mvP1HwSetting,
      .pPipelineNodesNeed = &(*pConfigInfo2)->mPipelineNodesNeed,
      .pStreamingFeatureSetting = &(*pConfigInfo2)->mStreamingFeatureSetting,
      .pCaptureFeatureSetting = &mConfigInfo2->mCaptureFeatureSetting,
      .batchSize = 0,
      .pOldPipelineContext = nullptr,
      .pDataCallback = shared_from_this(),
      .bUsingMultiThreadToBuildPipelineContext =
          mbUsingMultiThreadToBuildPipelineContext,
      .bIsReconfigure = true,
  };

  // Create New pipeline context for streaming.
  RETURN_ERROR_IF_NOT_OK(buildPipelineContext(&mCurrentPipelineContext, in),
                         "Fail on buildPipelineContext");

  MY_LOGD("requestNo(%d) processReconfigStream -", requestNo);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionStreaming::waitUntilP1NodeDrainedAndFlush() -> MERROR {
  MERROR err = OK;
  std::shared_ptr<PipelineContext> pPipelineContext =
      getCurrentPipelineContext();
  if (!pPipelineContext.get()) {
    MY_LOGW("get pPipelineContext fail");
    return UNKNOWN_ERROR;
  }
  MY_LOGD("waitUntilP1nodeDrainedAndFlush");
  // P1
  {
    std::shared_ptr<NSCam::v3::NSPipelineContext::NodeActor<NSCam::v3::P1Node>>
        nodeActor = nullptr;
    err = waitUntilNodeDrainedAndFlush(eNODEID_P1Node, &nodeActor);
    if (err != OK) {
      MY_LOGW("get wait until node(%d) drained and flush fail", eNODEID_P1Node);
      return err;
    }
  }
  return err;
}
/******************************************************************************
 *
 ******************************************************************************/
auto PipelineModelSessionStreaming::waitUntilP2DrainedAndFlush() -> MERROR {
  MERROR err = OK;
  std::shared_ptr<PipelineContext> pPipelineContext =
      getCurrentPipelineContext();
  if (!pPipelineContext.get()) {
    MY_LOGW("get pPipelineContext fail");
    return UNKNOWN_ERROR;
  }
  MY_LOGD("waitUntilP2DrainedAndFlush");
  // P2
  {
    std::shared_ptr<
        NSCam::v3::NSPipelineContext::NodeActor<NSCam::v3::P2StreamingNode>>
        nodeActor = nullptr;
    err = waitUntilNodeDrainedAndFlush(eNODEID_P2StreamNode, &nodeActor);
    if (err != OK) {
      MY_LOGW("get wait until node(%d) drained and flush fail",
              eNODEID_P2StreamNode);
      return err;
    }
  }

  return err;
}
/******************************************************************************
 *
 ******************************************************************************/
template <typename _Node_>
MERROR PipelineModelSessionStreaming::waitUntilNodeDrainedAndFlush(
    NodeId_T const nodeId,
    std::shared_ptr<NSCam::v3::NSPipelineContext::NodeActor<_Node_>>*
        pNodeActor) {
  std::shared_ptr<PipelineContext> pPipelineContext =
      getCurrentPipelineContext();
  if (!pPipelineContext.get()) {
    MY_LOGW("get pPipelineContext fail");
    return UNKNOWN_ERROR;
  }
  MERROR err = pPipelineContext->queryNodeActor(nodeId, pNodeActor);
  if (err != NAME_NOT_FOUND) {
    if (err != OK || *pNodeActor == nullptr) {
      MY_LOGW("get NodeActor(%" PRIdPTR ") fail", nodeId);
      return err;
    }
    //
    err = pPipelineContext->waitUntilNodeDrained(nodeId);
    if (err != OK) {
      MY_LOGW("wait until node(%" PRIdPTR ") drained fail", nodeId);
      return err;
    }
    //
    std::shared_ptr<IPipelineNode> node = (*pNodeActor)->getNode();
    if (node == nullptr) {
      MY_LOGW("get node(%" PRIdPTR ") fail", nodeId);
      return UNKNOWN_ERROR;
    }
    //
    err = node->flush();
    if (err != OK) {
      MY_LOGW("flush node(%" PRIdPTR ") fail", nodeId);
      return err;
    }
  }
  return OK;
}
