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

#define PIPE_CLASS_TAG "YUVNode"
#define PIPE_TRACE TRACE_YUV_NODE
#include <algorithm>
#include <common/include/PipeLog.h>
#include "../CaptureFeaturePlugin.h"
#include <memory>
#include <mtkcam/feature/featureCore/featurePipe/capture/nodes/YUVNode.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include <sstream>
#include <vector>

using NSCam::NSPipelinePlugin::eFD_Current;
using NSCam::NSPipelinePlugin::eImgSize_Full;
using NSCam::NSPipelinePlugin::eImgSize_Specified;
using NSCam::NSPipelinePlugin::MTK_FEATURE_ABF;
using NSCam::NSPipelinePlugin::MTK_FEATURE_NR;
using NSCam::NSPipelinePlugin::PipelinePlugin;
using NSCam::NSPipelinePlugin::PluginRegister;
using NSCam::NSPipelinePlugin::TP_FEATURE_FB;
using NSCam::NSPipelinePlugin::Yuv;
using NSCam::NSPipelinePlugin::YuvPlugin;

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

/******************************************************************************
 *
 ******************************************************************************/
class YuvInterface : public YuvPlugin::IInterface {
 public:
  virtual MERROR offer(YuvPlugin::Selection* sel) {
    sel->mIBufferFull.addSupportFormat(eImgFmt_NV12)
        .addSupportFormat(eImgFmt_YV12)
        .addSupportFormat(eImgFmt_YUY2)
        .addSupportFormat(eImgFmt_NV21)
        .addSupportFormat(eImgFmt_I420)
        .addSupportSize(eImgSize_Full);

    sel->mOBufferFull.addSupportFormat(eImgFmt_NV12)
        .addSupportFormat(eImgFmt_YV12)
        .addSupportFormat(eImgFmt_YUY2)
        .addSupportFormat(eImgFmt_NV21)
        .addSupportFormat(eImgFmt_I420)
        .addSupportSize(eImgSize_Full);

    sel->mOBufferCropA.addSupportFormat(eImgFmt_NV12)
        .addSupportFormat(eImgFmt_YV12)
        .addSupportFormat(eImgFmt_YUY2)
        .addSupportFormat(eImgFmt_NV21)
        .addSupportFormat(eImgFmt_I420)
        .addSupportSize(eImgSize_Specified);

    sel->mOBufferCropB.addSupportFormat(eImgFmt_NV12)
        .addSupportFormat(eImgFmt_YV12)
        .addSupportFormat(eImgFmt_YUY2)
        .addSupportFormat(eImgFmt_NV21)
        .addSupportFormat(eImgFmt_I420)
        .addSupportSize(eImgSize_Specified);

    return OK;
  }

  virtual ~YuvInterface() {}
};

REGISTER_PLUGIN_INTERFACE(Yuv, YuvInterface);

/******************************************************************************
 *
 ******************************************************************************/
class YuvCallback : public YuvPlugin::RequestCallback {
 public:
  explicit YuvCallback(std::shared_ptr<YUVNode> pNode) : mpNode(pNode) {}

  virtual void onAborted(YuvPlugin::Request::Ptr pPluginReq) {
    *pPluginReq = YuvPlugin::Request();
    MY_LOGD("onAborted request: %p", pPluginReq.get());
  }

  virtual void onCompleted(YuvPlugin::Request::Ptr pPluginReq, MERROR result) {
    RequestPtr pRequest = mpNode->findRequest(pPluginReq);

    if (pRequest == NULL) {
      MY_LOGE("unknown request happened: %p, result %d", pPluginReq.get(),
              result);
      return;
    }

    *pPluginReq = YuvPlugin::Request();
    MY_LOGD("onCompleted request:%p, result:%d", pPluginReq.get(), result);

    MBOOL bRepeat = mpNode->onRequestRepeat(pRequest);
    // no more repeating
    if (bRepeat) {
      mpNode->onRequestProcess(pRequest);
    } else {
      mpNode->onRequestFinish(pRequest);
    }
  }

  virtual ~YuvCallback() {}

 private:
  std::shared_ptr<YUVNode> mpNode;
};

YUVNode::YUVNode(NodeID_T nid, const char* name)
    : CaptureFeatureNode(nid, name) {
  TRACE_FUNC_ENTER();
  this->addWaitQueue(&mRequests);
  TRACE_FUNC_EXIT();
}

YUVNode::~YUVNode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MVOID YUVNode::setBufferPool(const std::shared_ptr<CaptureBufferPool>& pool) {
  TRACE_FUNC_ENTER();
  mpBufferPool = pool;
  TRACE_FUNC_EXIT();
}

MBOOL YUVNode::onData(DataID id, const RequestPtr& pRequest) {
  TRACE_FUNC_ENTER();
  MY_LOGD_IF(mLogLevel, "Frame %d: %s arrived", pRequest->getRequestNo(),
             PathID2Name(id));
  MBOOL ret = MTRUE;

  if (pRequest->isSatisfied(mNodeId)) {
    pRequest->addParameter(PID_REQUEST_REPEAT, 0);
    mRequests.enque(pRequest);
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL YUVNode::onInit() {
  TRACE_FUNC_ENTER();
  CaptureFeatureNode::onInit();

  mPlugin = YuvPlugin::getInstance(mSensorIndex);

  FeatureID_T featureId;
  auto& vpProviders = mPlugin->getProviders();
  mpInterface = mPlugin->getInterface();

  std::vector<ProviderPtr> vSortedProvider = vpProviders;

  std::sort(vSortedProvider.begin(), vSortedProvider.end(),
            [](const ProviderPtr& p1, const ProviderPtr& p2) {
              return p1->property().mPriority > p2->property().mPriority;
            });

  for (auto& pProvider : vSortedProvider) {
    const YuvPlugin::Property& rProperty = pProvider->property();
    featureId = NULL_FEATURE;

    if (mNodeId == NID_YUV && rProperty.mPosition != 0) {
      continue;
    }
    if (mNodeId == NID_YUV2 && rProperty.mPosition != 1) {
      continue;
    }

    if (rProperty.mFeatures & MTK_FEATURE_NR) {
      MY_LOGD("add MTK_FEATURE_NR");
      featureId = FID_NR;
    } else if (rProperty.mFeatures & MTK_FEATURE_ABF) {
      featureId = FID_ABF;
    } else if (rProperty.mFeatures & TP_FEATURE_FB) {
      featureId = FID_FB_3RD_PARTY;
    }

    if (featureId != NULL_FEATURE) {
      MY_LOGD_IF(mLogLevel, "%s finds plugin:%s, priority:%d",
                 NodeID2Name(mNodeId), FeatID2Name(featureId),
                 rProperty.mPriority);
      mProviderPairs.emplace_back();
      auto& item = mProviderPairs.back();
      item.mFeatureId = featureId;
      item.mpProvider = pProvider;

      pProvider->init();
    }
  }

  mpCallback = std::make_shared<YuvCallback>(
      std::dynamic_pointer_cast<YUVNode>(shared_from_this()));
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL YUVNode::onUninit() {
  TRACE_FUNC_ENTER();

  for (ProviderPair& p : mProviderPairs) {
    ProviderPtr pProvider = p.mpProvider;
    pProvider->uninit();
  }

  mProviderPairs.clear();

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL YUVNode::onThreadStart() {
  TRACE_FUNC_ENTER();
  MBOOL ret = MTRUE;
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL YUVNode::onThreadStop() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL YUVNode::onThreadLoop() {
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

  pRequest->mTimer.startYUV();
  incExtThreadDependency();
  onRequestProcess(pRequest);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL YUVNode::onRequestRepeat(RequestPtr& pRequest) {
  MINT32 repeat = pRequest->getParameter(PID_REQUEST_REPEAT);
  repeat++;
  std::shared_ptr<CaptureFeatureNodeRequest> pNodeReq =
      pRequest->getNodeRequest(mNodeId + repeat);

  // no more repeating
  if (pNodeReq == NULL) {
    return MFALSE;
  }

  MY_LOGD("onRequestRepeat request:%d, repeat:%d", pRequest->getRequestNo(),
          repeat);

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

  pRequest->addParameter(PID_REQUEST_REPEAT, repeat);

  return MTRUE;
}

MBOOL YUVNode::onRequestProcess(RequestPtr& pRequest) {
  MINT32 requestNo = pRequest->getRequestNo();
  MINT32 frameNo = pRequest->getFrameNo();
  MINT32 repeat = pRequest->getParameter(PID_REQUEST_REPEAT);
  NodeID_T uNodeId = mNodeId + repeat;
  CAM_TRACE_FMT_BEGIN("yuv(%d):process|r%df%d", mNodeId, requestNo, frameNo);
  MY_LOGD("(%d) +, R/F Num: %d/%d", mNodeId, requestNo, frameNo);

  std::shared_ptr<CaptureFeatureNodeRequest> pNodeReq =
      pRequest->getNodeRequest(uNodeId);
  if (pNodeReq == NULL) {
    MY_LOGE("should not be here if no node request");
    return MFALSE;
  }

  // pick a provider
  ProviderPtr pProvider = NULL;
  for (ProviderPair& p : mProviderPairs) {
    FeatureID_T featId = p.mFeatureId;
    if (pRequest->hasFeature(featId)) {
      if (repeat > 0) {
        repeat--;
        continue;
      }
      pProvider = p.mpProvider;
      break;
    }
  }

  if (pProvider == NULL) {
    MY_LOGE("do not execute a plugin");
    dispatch(pRequest);
    return MFALSE;
  }

  BufferID_T uIBufFull = pNodeReq->mapBufferID(TID_MAN_FULL_YUV, INPUT);
  BufferID_T uOBufFull = pNodeReq->mapBufferID(TID_MAN_FULL_YUV, OUTPUT);

  auto pPluginReq = mPlugin->createRequest();

  if (uIBufFull != NULL_BUFFER) {
    pPluginReq->mIBufferFull =
        std::make_shared<PluginBufferHandle>(pNodeReq, uIBufFull);
  }
  if (uOBufFull != NULL_BUFFER) {
    pPluginReq->mOBufferFull =
        std::make_shared<PluginBufferHandle>(pNodeReq, uOBufFull);
  }

  if (pNodeReq->hasMetadata(MID_MAN_IN_P1_DYNAMIC)) {
    pPluginReq->mIMetadataDynamic =
        std::make_shared<PluginMetadataHandle>(pNodeReq, MID_MAN_IN_P1_DYNAMIC);
  }
  if (pNodeReq->hasMetadata(MID_MAN_IN_APP)) {
    pPluginReq->mIMetadataApp =
        std::make_shared<PluginMetadataHandle>(pNodeReq, MID_MAN_IN_APP);
  }
  if (pNodeReq->hasMetadata(MID_MAN_IN_HAL)) {
    pPluginReq->mIMetadataHal =
        std::make_shared<PluginMetadataHandle>(pNodeReq, MID_MAN_IN_HAL);
  }
  if (pNodeReq->hasMetadata(MID_MAN_OUT_APP)) {
    pPluginReq->mOMetadataApp =
        std::make_shared<PluginMetadataHandle>(pNodeReq, MID_MAN_OUT_APP);
  }
  if (pNodeReq->hasMetadata(MID_MAN_OUT_HAL)) {
    pPluginReq->mOMetadataHal =
        std::make_shared<PluginMetadataHandle>(pNodeReq, MID_MAN_OUT_HAL);
  }

  MBOOL ret = MFALSE;
  {
    std::lock_guard<std::mutex> _l(mPairLock);
    mRequestPairs.emplace_back();
    auto& rPair = mRequestPairs.back();
    rPair.mPipe = pRequest;
    rPair.mPlugin = pPluginReq;
  }

  pProvider->process(pPluginReq, mpCallback);
  ret = MTRUE;

  MY_LOGD("(%d) -, R/F Num: %d/%d", mNodeId, requestNo, frameNo);
  CAM_TRACE_FMT_END();
  return ret;
}

RequestPtr YUVNode::findRequest(PluginRequestPtr& pPluginReq) {
  std::lock_guard<std::mutex> _l(mPairLock);
  for (const auto& rPair : mRequestPairs) {
    if (pPluginReq == rPair.mPlugin) {
      return rPair.mPipe;
    }
  }

  return NULL;
}

MBOOL YUVNode::onRequestFinish(RequestPtr& pRequest) {
  MINT32 requestNo = pRequest->getRequestNo();
  MINT32 frameNo = pRequest->getFrameNo();
  CAM_TRACE_FMT_BEGIN("yuv(%d):finish|r%df%d", mNodeId, requestNo, frameNo);
  MY_LOGD("(%d) +, R/F Num: %d/%d", mNodeId, requestNo, frameNo);

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

  pRequest->mTimer.stopYUV();
  dispatch(pRequest);

  decExtThreadDependency();
  CAM_TRACE_FMT_END();
  MY_LOGD("(%d) -, R/F Num: %d/%d", mNodeId, requestNo, frameNo);
  return MTRUE;
}

MERROR YUVNode::evaluate(CaptureFeatureInferenceData* rInfer) {
  if (rInfer->getRequestIndex() > 0) {
    // should not be involved to infer while blending frame
    return OK;
  }

  MBOOL isValid;
  MUINT8 uRepeatCount = 0;

  // Foreach all loaded plugin
  for (ProviderPair& p : mProviderPairs) {
    FeatureID_T featId = p.mFeatureId;

    if (!rInfer->hasFeature(featId)) {
      MY_LOGW(" no feature: %s", FeatID2Name(featId));
      continue;
    } else if (uRepeatCount >= 3) {
      MY_LOGE("over max repeating count(3), ignore feature: %s",
              FeatID2Name(featId));
      continue;
    }

    auto& rSrcData = rInfer->getSharedSrcData();
    auto& rDstData = rInfer->getSharedDstData();
    auto& rFeatures = rInfer->getSharedFeatures();
    auto& rMetadatas = rInfer->getSharedMetadatas();

    isValid = MTRUE;

    ProviderPtr pProvider = p.mpProvider;
    const YuvPlugin::Property& rProperty = pProvider->property();

    Selection sel;
    mpInterface->offer(&sel);
    sel.mIMetadataHal.setControl(rInfer->mpIMetadataHal);
    sel.mIMetadataApp.setControl(rInfer->mpIMetadataApp);
    sel.mIMetadataDynamic.setControl(rInfer->mpIMetadataDynamic);
    if (pProvider->negotiate(&sel) != OK) {
      MY_LOGD("bypass %s after negotiation", FeatID2Name(featId));
      rInfer->clearFeature(featId);
      continue;
    }

    // full size input
    if (sel.mIBufferFull.getRequired()) {
      if (sel.mIBufferFull.isValid()) {
        rSrcData.emplace_back();
        auto& src_0 = rSrcData.back();

        src_0.mTypeId = TID_MAN_FULL_YUV;
        if (!rInfer->hasType(TID_MAN_FULL_YUV)) {
          isValid = MFALSE;
        }

        src_0.mSizeId = sel.mIBufferFull.getSizes()[0];
        // Directly select the first format, using lazy strategy
        src_0.mFormat = sel.mIBufferFull.getFormats()[0];

        // In-place processing must add a output
        if (rProperty.mInPlace) {
          rDstData.emplace_back();
          auto& dst_0 = rDstData.back();
          dst_0.mTypeId = TID_MAN_FULL_YUV;
          dst_0.mSizeId = src_0.mSizeId;
          dst_0.mFormat = src_0.mFormat;
          dst_0.mSize = rInfer->getSize(TID_MAN_FULL_YUV);
          dst_0.mInPlace = true;
        }
      } else {
        isValid = MFALSE;
      }
    }

    // face detection (per-frame)
    if (rProperty.mFaceData == eFD_Current) {
      rSrcData.emplace_back();
      auto& src_1 = rSrcData.back();
      src_1.mTypeId = TID_MAN_FD;
      src_1.mSizeId = NULL_SIZE;
    }

    // full size output
    if (!rProperty.mInPlace && sel.mOBufferFull.getRequired()) {
      if (sel.mOBufferFull.isValid()) {
        rDstData.emplace_back();
        auto& dst_0 = rDstData.back();
        dst_0.mTypeId = TID_MAN_FULL_YUV;
        dst_0.mSizeId = sel.mOBufferFull.getSizes()[0];
        dst_0.mFormat = sel.mOBufferFull.getFormats()[0];
        dst_0.mSize = rInfer->getSize(TID_MAN_FULL_YUV);
      } else {
        isValid = MFALSE;
      }
    }

    if (sel.mIMetadataDynamic.getRequired()) {
      rMetadatas.push_back(MID_MAN_IN_P1_DYNAMIC);
    }

    if (sel.mIMetadataApp.getRequired()) {
      rMetadatas.push_back(MID_MAN_IN_APP);
    }

    if (sel.mIMetadataHal.getRequired()) {
      rMetadatas.push_back(MID_MAN_IN_HAL);
    }

    if (sel.mOMetadataApp.getRequired()) {
      rMetadatas.push_back(MID_MAN_OUT_APP);
    }

    if (sel.mOMetadataHal.getRequired()) {
      rMetadatas.push_back(MID_MAN_OUT_HAL);
    }

    if (isValid) {
      rFeatures.push_back(featId);
      rInfer->addNodeIO(mNodeId + uRepeatCount, rSrcData, rDstData, rMetadatas,
                        rFeatures);
      uRepeatCount++;
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
