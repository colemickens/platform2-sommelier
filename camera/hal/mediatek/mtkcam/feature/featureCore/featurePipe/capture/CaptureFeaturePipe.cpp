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

#define PIPE_CLASS_TAG "Pipe"
#define PIPE_TRACE TRACE_CAPTURE_FEATURE_PIPE
#include <capture/CaptureFeaturePipe.h>
#include <common/include/PipeLog.h>
#include <memory>
#include <vector>

#define NORMAL_STREAM_NAME "CaptureFeature"
#define THREAD_POSTFIX "@CapPipe"

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

CaptureFeaturePipe::CaptureFeaturePipe(MINT32 sensorIndex,
                                       const UsageHint& usageHint)
    : CamPipe<CaptureFeatureNode>("CaptureFeaturePipe"),
      mSensorIndex(sensorIndex) {
  TRACE_FUNC_ENTER();

  (void)usageHint;

  mLogLevel = ::property_get_int32("vendor.debug.camera.p2capture", 0);
  mForceImg3o422 =
      property_get_int32("vendor.debug.camera.p2.force.img3o.format422", 0) > 0;

  mpRootNode = std::make_shared<RootNode>(NID_ROOT, "Root" THREAD_POSTFIX);
  mpP2ANode = std::make_shared<P2ANode>(NID_P2A, "P2A" THREAD_POSTFIX);
  mpYUVNode = std::make_shared<YUVNode>(NID_YUV, "YUV" THREAD_POSTFIX);
  mpMDPNode = std::make_shared<MDPNode>(NID_MDP, "MDP" THREAD_POSTFIX);

  // Add node
  mpNodes.push_back(mpRootNode);
  mpNodes.push_back(mpP2ANode);
  mpNodes.push_back(mpMDPNode);
  // Sequence: BFS
  mInference.addNode(NID_P2A, mpP2ANode);
  mInference.addNode(NID_MDP, mpMDPNode);

  if (!mForceImg3o422) {
    mpNodes.push_back(mpYUVNode);
    mInference.addNode(NID_YUV, mpYUVNode);
  }

  mpCropCalculator = std::make_shared<CropCalculator>(mSensorIndex, mLogLevel);

  mNodeSignal = std::make_shared<NodeSignal>();
  if (mNodeSignal == NULL) {
    MY_LOGE("cannot create NodeSignal");
  }
  TRACE_FUNC_EXIT();
}

CaptureFeaturePipe::~CaptureFeaturePipe() {
  TRACE_FUNC_ENTER();

#define DELETE_NODE(node) \
  do {                    \
    if (node == nullptr)  \
      break;              \
    node.reset();         \
  } while (0)

  DELETE_NODE(mpRootNode);
  DELETE_NODE(mpP2ANode);
  DELETE_NODE(mpYUVNode);
  DELETE_NODE(mpMDPNode);
#undef DELETE_NODE

  // must call dispose to free CamGraph
  this->dispose();
  MY_LOGD("destroy pipe(%p): sensorIndex=%d", this, mSensorIndex);
  TRACE_FUNC_EXIT();
}

void CaptureFeaturePipe::setSensorIndex(MINT32 sensorIndex) {
  TRACE_FUNC_ENTER();
  this->mSensorIndex = sensorIndex;
  TRACE_FUNC_EXIT();
}

MVOID CaptureFeaturePipe::init() {
  TRACE_FUNC_ENTER();
  MBOOL ret;
  ret = PARENT_PIPE::init();
  TRACE_FUNC_EXIT();
}

MBOOL CaptureFeaturePipe::config(const StreamConfigure config) {
  mpP2ANode->configNormalStream(config);
  return MTRUE;
}

MVOID CaptureFeaturePipe::uninit() {
  TRACE_FUNC_ENTER();
  MBOOL ret;
  ret = PARENT_PIPE::uninit();
  TRACE_FUNC_EXIT();
}

MERROR CaptureFeaturePipe::enque(
    std::shared_ptr<ICaptureFeatureRequest> request) {
  TRACE_FUNC_ENTER();
  MBOOL ret = MFALSE;
  if (request == nullptr) {
    MY_LOGE("enque an empty request!");
    TRACE_FUNC_EXIT();
    return BAD_VALUE;
  } else {
    std::shared_ptr<CaptureFeatureRequest> pRequest(
        std::dynamic_pointer_cast<CaptureFeatureRequest>(request));
    // For the event of next capture
    pRequest->mpCallback = mpCallback;

    mInference.evaluate(pRequest);

    CaptureFeatureRequest& rRequest = *pRequest;

    for (auto& it : rRequest.mBufferItems) {
      auto& key = it.first;
      auto& item = it.second;

      if (!item.mCreated && !!item.mSize) {
        rRequest.mBufferMap.emplace(
            key, new PipeBufferHandle(mpBufferPool, item.mFormat, item.mSize));
        item.mCreated = MTRUE;
      }
    }
#ifdef __DEBUG
    pRequest->dump();
#endif
    mNodeSignal->clearStatus(NodeSignal::STATUS_IN_FLUSH);
    ret = CamPipe::enque(PID_ENQUE, &pRequest);
  }
  TRACE_FUNC_EXIT();
  return ret ? OK : UNKNOWN_ERROR;
}

MBOOL CaptureFeaturePipe::flush() {
  TRACE_FUNC_ENTER();
  MY_LOGD("Trigger flush");
  mNodeSignal->setStatus(NodeSignal::STATUS_IN_FLUSH);
  CamPipe::sync();
  mNodeSignal->clearStatus(NodeSignal::STATUS_IN_FLUSH);
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL CaptureFeaturePipe::onInit() {
  TRACE_FUNC_ENTER();
  MBOOL ret;
  ret = this->prepareNodeSetting() && this->prepareNodeConnection() &&
        this->prepareBuffer();

  TRACE_FUNC_EXIT();
  return ret;
}

MVOID CaptureFeaturePipe::onUninit() {
  TRACE_FUNC_ENTER();
  this->releaseBuffer();
  this->releaseNodeSetting();
  TRACE_FUNC_EXIT();
}

MBOOL CaptureFeaturePipe::onData(DataID id, const RequestPtr& pRequest) {
  TRACE_FUNC_ENTER();
  (void)id;

  MY_LOGD("R/F Num: %d/%d - Finished", pRequest->getRequestNo(),
          pRequest->getFrameNo());

  mpCallback->onCompleted(pRequest, OK);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL CaptureFeaturePipe::prepareNodeSetting() {
  TRACE_FUNC_ENTER();
  NODE_LIST::iterator it, end;
  for (it = mpNodes.begin(), end = mpNodes.end(); it != end; ++it) {
    (*it)->setSensorIndex(mSensorIndex);
    (*it)->setNodeSignal(mNodeSignal);
    (*it)->setCropCalculator(mpCropCalculator);
    if (mLogLevel > 0) {
      (*it)->setLogLevel(mLogLevel);
    }
  }

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL CaptureFeaturePipe::prepareNodeConnection() {
  TRACE_FUNC_ENTER();
  this->connectData(PID_ROOT_TO_P2A, mpRootNode, mpP2ANode);
  this->connectData(PID_P2A_TO_MDP, mpP2ANode, mpMDPNode);
  if (!mForceImg3o422) {
    this->connectData(PID_P2A_TO_YUV, mpP2ANode, mpYUVNode);
    this->connectData(PID_YUV_TO_MDP, mpYUVNode, mpMDPNode);
  }
  NODE_LIST::iterator it, end;
  for (it = mpNodes.begin(), end = mpNodes.end(); it != end; ++it) {
    this->connectData(PID_DEQUE, PID_DEQUE, &(*it), shared_from_this());
  }

  this->setRootNode(mpRootNode);
  mpRootNode->registerInputDataID(PID_ENQUE);

  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL CaptureFeaturePipe::prepareBuffer() {
  TRACE_FUNC_ENTER();
  mpBufferPool = std::make_shared<CaptureBufferPool>("fpipe");
  mpBufferPool->init(std::vector<BufferConfig>());

  mpP2ANode->setBufferPool(mpBufferPool);
  TRACE_FUNC_EXIT();
  return MTRUE;
}

std::shared_ptr<IBufferPool> CaptureFeaturePipe::createFullImgPool(
    const char* name, MSize size) {
  TRACE_FUNC_ENTER();

  std::shared_ptr<IBufferPool> fullImgPool;
  NSCam::EImageFormat format = eImgFmt_YV12;
  fullImgPool = ImageBufferPool::create(name, size.w, size.h, format,
                                        ImageBufferPool::USAGE_HW);

  TRACE_FUNC_EXIT();

  return fullImgPool;
}

MVOID CaptureFeaturePipe::releaseNodeSetting() {
  TRACE_FUNC_ENTER();
  this->disconnect();
  TRACE_FUNC_EXIT();
}

MVOID CaptureFeaturePipe::releaseBuffer() {
  TRACE_FUNC_ENTER();

  mpP2ANode->setBufferPool(NULL);

  TRACE_FUNC_EXIT();
}

std::shared_ptr<ICaptureFeatureRequest> CaptureFeaturePipe::acquireRequest() {
  auto pRequest = std::make_shared<CaptureFeatureRequest>();
  return pRequest;
}

MVOID CaptureFeaturePipe::releaseRequest(
    std::shared_ptr<ICaptureFeatureRequest>) {}

MVOID CaptureFeaturePipe::setCallback(
    std::shared_ptr<RequestCallback> pCallback) {
  mpCallback = pCallback;
}

PipeBufferHandle::PipeBufferHandle(
    std::shared_ptr<CaptureBufferPool> pBufferPool,
    Format_T format,
    const MSize& size)
    : mpBufferPool(pBufferPool), mFormat(format), mSize(size) {}

MERROR PipeBufferHandle::acquire(MINT usage) {
  (void)usage;

  MY_LOGD("allocate image buffer(%dx%d) format(%d)", mSize.w, mSize.h, mFormat);

  mpSmartBuffer = mpBufferPool->getImageBuffer(mSize.w, mSize.h, mFormat);
  return OK;
}

IImageBuffer* PipeBufferHandle::native() {
  if (mpSmartBuffer.get() == nullptr) {
    return nullptr;
  }

  return mpSmartBuffer->mImageBuffer.get();
}

MVOID PipeBufferHandle::release() {}

MVOID PipeBufferHandle::dump(std::ostream& os) const {
  (void)os;
}

MUINT32 PipeBufferHandle::getTransform() {
  return 0;
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
