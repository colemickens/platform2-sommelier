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

#define LOG_TAG "MtkCam/ppl_builder"
//
#include <memory>
#include "MyUtils.h"
#include "PipelineContextImpl.h"
//
using NSCam::v3::IPipelineFrame;
using NSCam::v3::NSPipelineContext::NodeBuilder;
using NSCam::v3::NSPipelineContext::PipelineBuilder;
using NSCam::v3::NSPipelineContext::RequestBuilder;
using NSCam::v3::NSPipelineContext::StreamBuilder;

/******************************************************************************
 *
 ******************************************************************************/
StreamBuilder::StreamBuilder(eStreamType const type,
                             std::shared_ptr<IImageStreamInfo> pStreamInfo)
    : mpImpl(new StreamBuilderImpl()) {
  mpImpl->mType = type;
  mpImpl->mpImageStreamInfo = pStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
StreamBuilder::StreamBuilder(eStreamType const type,
                             std::shared_ptr<IMetaStreamInfo> pStreamInfo)
    : mpImpl(new StreamBuilderImpl()) {
  mpImpl->mType = type;
  mpImpl->mpMetaStreamInfo = pStreamInfo;
}

/******************************************************************************
 *
 ******************************************************************************/
StreamBuilder::StreamBuilder(const StreamBuilder& builder) {
  mpImpl = builder.mpImpl;
}

/******************************************************************************
 *
 ******************************************************************************/
StreamBuilder::~StreamBuilder() {}

/******************************************************************************
 *
 ******************************************************************************/
StreamBuilder& StreamBuilder::setProvider(
    std::shared_ptr<IStreamBufferProviderT> pProvider) {
  mpImpl->mpProvider = pProvider;
  return *this;
}
/******************************************************************************
 *
 ******************************************************************************/
MERROR
StreamBuilder::build(std::shared_ptr<PipelineContext> pContext) const {
  typedef PipelineContext::PipelineContextImpl PipelineContextImplT;
  PipelineContextImplT* pContextImpl;
  if (!pContext.get() || !(pContextImpl = pContext->getImpl())) {
    MY_LOGE("cannot get context");
    return UNKNOWN_ERROR;
  }
  //
  MERROR err;
  if (NSCam::OK != (err = pContextImpl->updateConfig(mpImpl.get()))) {
    return err;
  }
  //
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
NodeBuilder::NodeBuilder(NodeId_T const nodeId,
                         std::shared_ptr<INodeActor> pNode)
    : mpImpl(std::make_shared<NodeBuilderImpl>(nodeId, pNode)) {}

/******************************************************************************
 *
 ******************************************************************************/
NodeBuilder::NodeBuilder(const NodeBuilder& builder) {
  mpImpl = builder.mpImpl;
}

/******************************************************************************
 *
 ******************************************************************************/
NodeBuilder::~NodeBuilder() {}

/******************************************************************************
 *
 ******************************************************************************/
NodeBuilder& NodeBuilder::addStream(eDirection const direction,
                                    StreamSet const& streams) {
  if (direction == eDirection_IN) {
    mpImpl->mInStreamSet.add(streams);
  } else if (direction == eDirection_OUT) {
    mpImpl->mOutStreamSet.add(streams);
  }
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
NodeBuilder& NodeBuilder::setImageStreamUsage(StreamId_T const streamId,
                                              MUINT const bufUsage) {
  mpImpl->mUsageMap.emplace(streamId, bufUsage);
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NodeBuilder::build(std::shared_ptr<PipelineContext> pContext) const {
  typedef PipelineContext::PipelineContextImpl PipelineContextImplT;
  PipelineContextImplT* pContextImpl;
  if (!pContext.get() || !(pContextImpl = pContext->getImpl())) {
    MY_LOGE("cannot get context");
    return UNKNOWN_ERROR;
  }
  // 1. check if this Node is already marked as reuse
  // 2. create Node if not existed
  MERROR err;
  if (NSCam::OK != (err = pContextImpl->updateConfig(mpImpl.get()))) {
    return err;
  }
  //
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineBuilder::PipelineBuilder()
    : mpImpl(std::make_shared<PipelineBuilderImpl>()) {}

/******************************************************************************
 *
 ******************************************************************************/
PipelineBuilder::PipelineBuilder(const PipelineBuilder& builder) {
  mpImpl = builder.mpImpl;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineBuilder::~PipelineBuilder() {}

/******************************************************************************
 *
 ******************************************************************************/
PipelineBuilder& PipelineBuilder::setRootNode(NodeSet const& roots) {
  mpImpl->mRootNodes.add(roots);
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineBuilder& PipelineBuilder::setNodeEdges(NodeEdgeSet const& edges) {
  mpImpl->mNodeEdges = edges;
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineBuilder::build(std::shared_ptr<PipelineContext> pContext) const {
  typedef PipelineContext::PipelineContextImpl PipelineContextImplT;
  PipelineContextImplT* pContextImpl;
  if (!pContext.get() || !(pContextImpl = pContext->getImpl())) {
    MY_LOGE("cannot get context");
    return UNKNOWN_ERROR;
  }
  //
  MERROR err;
  if (NSCam::OK != (err = pContextImpl->updateConfig(mpImpl.get()))) {
    return err;
  }
  //
  return NSCam::OK;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder::RequestBuilder() : mpImpl(new RequestBuilderImpl()) {}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder::~RequestBuilder() {}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::setReprocessFrame(MBOOL const bReprocessFrame) {
  mpImpl->mbReprocessFrame = bReprocessFrame;
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::setIOMap(NodeId_T const nodeId,
                                         IOMapSet const& imageIOMap,
                                         IOMapSet const& metaIOMap) {
  if (!mpImpl->getFlag(RequestBuilderImpl::FLAG_IOMAP_CHANGED)) {
    mpImpl->mImageNodeIOMaps.clear();
    mpImpl->mMetaNodeIOMaps.clear();
    //
    mpImpl->setFlag(RequestBuilderImpl::FLAG_IOMAP_CHANGED);
  }
  mpImpl->mImageNodeIOMaps.emplace(nodeId, imageIOMap);
  mpImpl->mMetaNodeIOMaps.emplace(nodeId, metaIOMap);
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::setRootNode(NodeSet const& roots) {
  mpImpl->setFlag(RequestBuilderImpl::FLAG_NODEEDGE_CHANGED);
  mpImpl->mRootNodes = roots;
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::setNodeEdges(NodeEdgeSet const& edges) {
  mpImpl->setFlag(RequestBuilderImpl::FLAG_NODEEDGE_CHANGED);
  mpImpl->mNodeEdges = edges;
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::replaceStreamInfo(
    StreamId_T const streamId, std::shared_ptr<IImageStreamInfo> pStreamInfo) {
  mpImpl->setFlag(RequestBuilderImpl::FLAG_REPLACE_STREAMINFO);
  mpImpl->mReplacingInfos.emplace(streamId, pStreamInfo);
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::setImageStreamBuffer(
    StreamId_T const streamId, std::shared_ptr<IImageStreamBuffer> buffer) {
  mpImpl->mStreamBuffers_Image.emplace(streamId, buffer);
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::setImageStreamBuffer(
    StreamId_T const streamId, std::shared_ptr<HalImageStreamBuffer> buffer) {
  mpImpl->mStreamBuffers_HalImage.emplace(streamId, buffer);
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::setMetaStreamBuffer(
    StreamId_T const streamId, std::shared_ptr<IMetaStreamBuffer> buffer) {
  mpImpl->mStreamBuffers_Meta.emplace(streamId, buffer);
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::setMetaStreamBuffer(
    StreamId_T const streamId, std::shared_ptr<HalMetaStreamBuffer> buffer) {
  mpImpl->mStreamBuffers_HalMeta.emplace(streamId, buffer);
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
RequestBuilder& RequestBuilder::updateFrameCallback(
    std::weak_ptr<AppCallbackT> pCallback) {
  mpImpl->setFlag(RequestBuilderImpl::FLAG_CALLBACK_CHANGED);
  mpImpl->mpCallback = pCallback;
  return *this;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IPipelineFrame> RequestBuilder::build(
    MUINT32 const requestNo, std::shared_ptr<PipelineContext> pContext) {
  FUNC_START;
  typedef PipelineContext::PipelineContextImpl PipelineContextImplT;
  PipelineContextImplT* pContextImpl;
  if (!pContext.get() || !(pContextImpl = pContext->getImpl())) {
    MY_LOGE("cannot get context");
    return nullptr;
  }
  //
  MY_LOGD("build requestNo %d", requestNo);
  std::shared_ptr<IPipelineFrame> pFrame =
      pContextImpl->constructRequest(mpImpl.get(), requestNo);
  if (!pFrame.get()) {
    MY_LOGE("constructRequest failed");
  }
  //
  FUNC_END;
  return pFrame;
}

/******************************************************************************
 *
 ******************************************************************************/
