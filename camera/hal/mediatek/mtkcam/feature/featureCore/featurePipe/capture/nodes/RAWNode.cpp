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

#include <common/include/DebugControl.h>

#define PIPE_CLASS_TAG "RAWNode"
#define PIPE_TRACE TRACE_RAW_NODE
#include <capture/nodes/RAWNode.h>
#include <common/include/PipeLog.h>
#include <memory>
#include <sstream>
#include "../CaptureFeaturePlugin.h"
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>

using NSCam::NSPipelinePlugin::eImgSize_Full;
using NSCam::NSPipelinePlugin::MTK_FEATURE_REMOSAIC;
using NSCam::NSPipelinePlugin::PipelinePlugin;
using NSCam::NSPipelinePlugin::PluginRegister;
using NSCam::NSPipelinePlugin::Raw;
using NSCam::NSPipelinePlugin::RawPlugin;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

/******************************************************************************
 *
 ******************************************************************************/
class RawInterface : public RawPlugin::IInterface {
 public:
  virtual MERROR offer(RawPlugin::Selection* sel) {
    sel->mIBufferFull.addSupportFormat(eImgFmt_BAYER10)
        .addSupportFormat(eImgFmt_BAYER10_UNPAK)
        .addSupportSize(eImgSize_Full);

    sel->mOBufferFull.addSupportFormat(eImgFmt_BAYER10)
        .addSupportFormat(eImgFmt_BAYER10_UNPAK)
        .addSupportSize(eImgSize_Full);

    return OK;
  }

  virtual ~RawInterface() {}
};

REGISTER_PLUGIN_INTERFACE(Raw, RawInterface);

/******************************************************************************
 *
 ******************************************************************************/
class RawCallback : public RawPlugin::RequestCallback {
 public:
  explicit RawCallback(std::shared_ptr<RAWNode> pNode) : mpNode(pNode) {}

  virtual void onAborted(RawPlugin::Request::Ptr pPluginReq) {
    *pPluginReq = RawPlugin::Request();
    MY_LOGD("onAborted request: %p", pPluginReq.get());
  }

  virtual void onCompleted(RawPlugin::Request::Ptr pPluginReq, MERROR result) {
    RequestPtr pRequest = mpNode->findRequest(pPluginReq);

    if (pRequest == NULL) {
      MY_LOGE("unknown request happened: %p, result %d", pPluginReq.get(),
              result);
      return;
    }

    *pPluginReq = RawPlugin::Request();
    MY_LOGD("onCompleted request:%p, result:%d", pPluginReq.get(), result);

    mpNode->onRequestFinish(pRequest);
  }

  virtual ~RawCallback() {}

 private:
  std::shared_ptr<RAWNode> mpNode;
};

RAWNode::RAWNode(NodeID_T nid, const char* name)
    : CaptureFeatureNode(nid, name) {
  TRACE_FUNC_ENTER();
  this->addWaitQueue(&mRequests);
  TRACE_FUNC_EXIT();
}

RAWNode::~RAWNode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MVOID RAWNode::setBufferPool(const std::shared_ptr<CaptureBufferPool>& pool) {
  TRACE_FUNC_ENTER();
  mpBufferPool = pool;
  TRACE_FUNC_EXIT();
}

MBOOL RAWNode::onData(DataID id, const RequestPtr& pRequest) {
  TRACE_FUNC_ENTER();
  MY_LOGD_IF(mLogLevel, "Frame %d: %s arrived", pRequest->getRequestNo(),
             PathID2Name(id));
  MBOOL ret = MTRUE;

  if (pRequest->isSatisfied(mNodeId)) {
    mRequests.enque(pRequest);
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL RAWNode::onInit() {
  TRACE_FUNC_ENTER();
  CaptureFeatureNode::onInit();

  mPlugin = RawPlugin::getInstance(mSensorIndex);

  FeatureID_T featureId;
  auto& vpProviders = mPlugin->getProviders();
  mpInterface = mPlugin->getInterface();

  for (auto& pProvider : vpProviders) {
    const RawPlugin::Property& rProperty = pProvider->property();
    featureId = NULL_FEATURE;

    if (rProperty.mFeatures & MTK_FEATURE_REMOSAIC) {
      featureId = FID_REMOSAIC;
    }

    if (featureId != NULL_FEATURE) {
      MY_LOGD_IF(mLogLevel, "%s finds plugin:%s", NodeID2Name(mNodeId),
                 FeatID2Name(featureId));
      mProviderPairs.emplace_back();
      auto& item = mProviderPairs.back();
      item.mFeatureId = featureId;
      item.mpProvider = pProvider;

      pProvider->init();
    }
  }

  mpCallback = std::make_shared<RawCallback>(
      std::dynamic_pointer_cast<RAWNode>(shared_from_this()));
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL RAWNode::onUninit() {
  TRACE_FUNC_ENTER();

  for (ProviderPair& p : mProviderPairs) {
    ProviderPtr pProvider = p.mpProvider;
    pProvider->uninit();
  }

  mProviderPairs.clear();

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL RAWNode::onThreadStart() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL RAWNode::onThreadStop() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL RAWNode::onThreadLoop() {
  TRACE_FUNC_ENTER();
  if (!waitAllQueue()) {
    TRACE_FUNC("Wait all queue exit");
    return MFALSE;
  }

  RequestPtr pRequest;
  if (!mRequests.deque(&pRequest)) {
    MY_LOGE("Request deque out of sync");
    return MFALSE;
  } else if (pRequest == NULL) {
    MY_LOGE("Request out of sync");
    return MFALSE;
  }

  pRequest->mTimer.startRAW();
  onRequestProcess(pRequest);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL RAWNode::onRequestProcess(RequestPtr& pRequest) {
  MINT32 requestNo = pRequest->getRequestNo();
  MINT32 frameNo = pRequest->getFrameNo();
  CAM_TRACE_FMT_BEGIN("raw:process|r%df%d", requestNo, frameNo);
  MY_LOGD("+, R/F Num: %d/%d", requestNo, frameNo);

  std::shared_ptr<CaptureFeatureNodeRequest> pNodeReq =
      pRequest->getNodeRequest(mNodeId);
  if (pNodeReq == NULL) {
    MY_LOGE("should not be here if no node request");
    return MFALSE;
  }

  // pick a provider
  ProviderPtr pProvider = NULL;
  for (ProviderPair& p : mProviderPairs) {
    FeatureID_T featId = p.mFeatureId;
    if (pRequest->hasFeature(featId)) {
      pProvider = p.mpProvider;
      break;
    }
  }

  if (pProvider == NULL) {
    MY_LOGE("do not execute a plugin");
    dispatch(pRequest);
    return MFALSE;
  }

  BufferID_T uIBufFull = pNodeReq->mapBufferID(TID_MAIN_FULL_RAW, INPUT);
  BufferID_T uOBufFull = pNodeReq->mapBufferID(TID_MAIN_FULL_RAW, OUTPUT);

  auto pPluginReq = mPlugin->createRequest();
  MBOOL isInPlace = pProvider->property().mInPlace;

  if (uIBufFull != NULL_BUFFER) {
    pPluginReq->mIBufferFull =
        std::make_shared<PluginBufferHandle>(pNodeReq, uIBufFull);
  }
  if (!isInPlace && uOBufFull != NULL_BUFFER) {
    pPluginReq->mOBufferFull =
        std::make_shared<PluginBufferHandle>(pNodeReq, uOBufFull);
  }

  if (pNodeReq->hasMetadata(MID_MAIN_IN_P1_DYNAMIC)) {
    pPluginReq->mIMetadataDynamic = std::make_shared<PluginMetadataHandle>(
        pNodeReq, MID_MAIN_IN_P1_DYNAMIC);
  }
  if (pNodeReq->hasMetadata(MID_MAIN_IN_APP)) {
    pPluginReq->mIMetadataApp =
        std::make_shared<PluginMetadataHandle>(pNodeReq, MID_MAIN_IN_APP);
  }
  if (pNodeReq->hasMetadata(MID_MAIN_IN_HAL)) {
    pPluginReq->mIMetadataHal =
        std::make_shared<PluginMetadataHandle>(pNodeReq, MID_MAIN_IN_HAL);
  }
  if (pNodeReq->hasMetadata(MID_MAIN_OUT_APP)) {
    pPluginReq->mOMetadataApp =
        std::make_shared<PluginMetadataHandle>(pNodeReq, MID_MAIN_OUT_APP);
  }
  if (pNodeReq->hasMetadata(MID_MAIN_OUT_HAL)) {
    pPluginReq->mOMetadataHal =
        std::make_shared<PluginMetadataHandle>(pNodeReq, MID_MAIN_OUT_HAL);
  }

  {
    std::lock_guard<std::mutex> _l(mPairLock);
    mRequestPairs.emplace_back();
    auto& rPair = mRequestPairs.back();
    rPair.mPipe = pRequest;
    rPair.mPlugin = pPluginReq;
  }

  incExtThreadDependency();
  if (pProvider->process(pPluginReq, mpCallback) != OK) {
    onRequestFinish(pRequest);
    return MFALSE;
  }

  MY_LOGD("-, R/F Num: %d/%d", requestNo, frameNo);
  CAM_TRACE_FMT_END();
  return MTRUE;
}

RequestPtr RAWNode::findRequest(PluginRequestPtr& pPluginReq) {
  std::lock_guard<std::mutex> _l(mPairLock);
  for (const auto& rPair : mRequestPairs) {
    if (pPluginReq == rPair.mPlugin) {
      return rPair.mPipe;
    }
  }

  return NULL;
}

MBOOL RAWNode::onRequestFinish(RequestPtr& pRequest) {
  MINT32 requestNo = pRequest->getRequestNo();
  MINT32 frameNo = pRequest->getFrameNo();
  CAM_TRACE_FMT_BEGIN("raw:finish|r%df%d", requestNo, frameNo);
  MY_LOGD("+, R/F Num: %d/%d", requestNo, frameNo);

  {
    std::lock_guard<std::mutex> _l(mPairLock);
    auto it = mRequestPairs.begin();
    for (; it != mRequestPairs.end(); it++) {
      if ((*it).mPipe == pRequest) {
        mRequestPairs.erase(it);
        break;
      }
    }
  }

  pRequest->mTimer.stopRAW();
  dispatch(pRequest);

  decExtThreadDependency();
  CAM_TRACE_FMT_END();
  MY_LOGD("-, R/F Num: %d/%d", requestNo, frameNo);
  return MTRUE;
}

MERROR RAWNode::evaluate(CaptureFeatureInferenceData* rInfer) {
  MBOOL isValid;
  MBOOL isEvaluated = MFALSE;

  // Foreach all loaded plugin
  for (ProviderPair& p : mProviderPairs) {
    FeatureID_T featId = p.mFeatureId;

    if (!rInfer->hasFeature(featId)) {
      continue;
    } else if (isEvaluated) {
      MY_LOGE("has duplicated feature: %s", FeatID2Name(featId));
      continue;
    }

    auto& rSrcData = rInfer->getSharedSrcData();
    auto& rDstData = rInfer->getSharedDstData();
    auto& rFeatures = rInfer->getSharedFeatures();
    auto& metadatas = rInfer->getSharedMetadatas();

    isValid = MTRUE;

    ProviderPtr pProvider = p.mpProvider;
    const RawPlugin::Property& rProperty = pProvider->property();

    // should get selection from camera setting
    auto pSelection = mPlugin->popSelection(pProvider);
    if (pSelection == NULL) {
      MY_LOGE("can not pop the selection, feature:%s", FeatID2Name(featId));
      rInfer->clearFeature(featId);
      return BAD_VALUE;
    }
    const Selection& sel = *pSelection;
    // full size input
    if (sel.mIBufferFull.getRequired()) {
      if (sel.mIBufferFull.isValid()) {
        rSrcData.emplace_back();
        auto& src_0 = rSrcData.back();

        src_0.mTypeId = TID_MAIN_FULL_RAW;
        if (!rInfer->hasType(TID_MAIN_FULL_RAW)) {
          isValid = MFALSE;
        }

        src_0.mSizeId = sel.mIBufferFull.getSizes()[0];
        // Directly select the first format, using lazy strategy
        src_0.mFormat = sel.mIBufferFull.getFormats()[0];

        // In-place processing must add a output
        if (rProperty.mInPlace) {
          rDstData.emplace_back();
          auto& dst_0 = rDstData.back();
          dst_0.mTypeId = TID_MAIN_FULL_RAW;
          dst_0.mSizeId = src_0.mSizeId;
          dst_0.mFormat = src_0.mFormat;
          dst_0.mSize = rInfer->getSize(TID_MAIN_FULL_RAW);
          dst_0.mInPlace = true;
        }
      } else {
        isValid = MFALSE;
      }
    }

    // full size output
    if (!rProperty.mInPlace && sel.mOBufferFull.getRequired()) {
      if (sel.mOBufferFull.isValid()) {
        rDstData.emplace_back();
        auto& dst_0 = rDstData.back();
        dst_0.mTypeId = TID_MAIN_FULL_RAW;
        dst_0.mSizeId = sel.mOBufferFull.getSizes()[0];
        dst_0.mFormat = sel.mOBufferFull.getFormats()[0];
        dst_0.mSize = rInfer->getSize(TID_MAIN_FULL_RAW);
      } else {
        isValid = MFALSE;
      }
    }

    if (sel.mIMetadataDynamic.getRequired()) {
      metadatas.push_back(MID_MAIN_IN_P1_DYNAMIC);
    }

    if (sel.mIMetadataApp.getRequired()) {
      metadatas.push_back(MID_MAIN_IN_APP);
    }

    if (sel.mIMetadataHal.getRequired()) {
      metadatas.push_back(MID_MAIN_IN_HAL);
    }

    if (sel.mIMetadataApp.getRequired()) {
      metadatas.push_back(MID_MAIN_OUT_APP);
    }

    if (sel.mOMetadataHal.getRequired()) {
      metadatas.push_back(MID_MAIN_OUT_HAL);
    }

    if (isValid) {
      isEvaluated = MTRUE;
      rFeatures.push_back(featId);
      rInfer->addNodeIO(mNodeId, rSrcData, rDstData, metadatas, rFeatures);
    } else {
      MY_LOGW("%s has invalid evaluation:%s", NodeID2Name(mNodeId),
              FeatID2Name(featId));
    }
  }

  return OK;
}
}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
