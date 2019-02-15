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

#define LOG_TAG "MtkCam/ppl_context"
//
#include <memory>
#include "MyUtils.h"
#include "PipelineContextImpl.h"
#include <property_lib.h>
#include <string>
#include <vector>
//
using NSCam::v3::NSPipelineContext::ContextNode;
using NSCam::v3::NSPipelineContext::DispatcherBase;
using NSCam::v3::NSPipelineContext::HalImageStreamBufferPoolT;
using NSCam::v3::NSPipelineContext::INodeActor;
using NSCam::v3::NSPipelineContext::IOMapSet;
using NSCam::v3::NSPipelineContext::PipelineContext;

/******************************************************************************
 *
 ******************************************************************************/
const IOMapSet& IOMapSet::buildEmptyIOMap() {
  static IOMapSet o;
  return o;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<PipelineContext> PipelineContext::create(char const* name) {
  return std::make_shared<PipelineContext>(name);
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineContext::PipelineContext(char const* name)
    : mpImpl(std::make_shared<PipelineContextImpl>(name)) {}

/******************************************************************************
 *
 ******************************************************************************/
PipelineContext::~PipelineContext() {
  MY_LOGD("deconstruction");
  onLastStrongRef(nullptr);
}

/******************************************************************************
 *
 ******************************************************************************/
void PipelineContext::onLastStrongRef(const void* /*id*/) {
  std::string name(mpImpl->getName());
  MY_LOGD("name: %s(%p) +", name.c_str(), this);
  mpImpl = nullptr;
  MY_LOGD("name: %s -", name.c_str());
}

/******************************************************************************
 *
 ******************************************************************************/
char const* PipelineContext::getName() const {
  return mpImpl->getName();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::beginConfigure(std::shared_ptr<PipelineContext> oldContext) {
  FUNC_START;
  // if old == current
  if (oldContext.get() == this) {
    MY_LOGW("context: old == this");
    return INVALID_OPERATION;
  }
  waitUntilDrained();

  mpOldContext = oldContext;
  FUNC_END;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::endConfigure(MBOOL const parallelConfig) {
  MERROR err;
  // DEBUG
  int Enable_Dump = property_get_int32("vendor.pipeline.state.dump", 0);
  if (Enable_Dump) {
    dump();
  }
  //
  if (OK !=
      (err = getImpl()->config(mpOldContext ? mpOldContext->getImpl() : nullptr,
                               parallelConfig))) {
    MY_LOGE("config fail");
    return err;
  }
  // release old context
  mpOldContext = nullptr;

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::queue(std::shared_ptr<IPipelineFrame> const& pFrame) {
  return getImpl()->queue(pFrame);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::kick(std::shared_ptr<IPipelineFrame> const& pFrame) {
  return getImpl()->kick(pFrame);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::flush() {
  FUNC_START;
  // begin flush
  getImpl()->beginFlush();
  //
  // wait until drained
  getImpl()->waitUntilDrained();
  //
  // end flush
  getImpl()->endFlush();
  //
  FUNC_END;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::waitUntilDrained() {
  FUNC_START;
  //
  getImpl()->waitUntilDrained();
  //
  FUNC_END;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::waitUntilNodeDrained(NodeId_T const nodeId) {
  FUNC_START;
  //
  getImpl()->waitUntilNodeDrained(nodeId);
  //
  FUNC_END;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::setScenarioControl(
    std::shared_ptr<NSCam::IScenarioControl> pControl) {
  return getImpl()->setScenarioControl(pControl);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::IScenarioControl> PipelineContext::getScenarioControl()
    const {
  return getImpl()->getScenarioControl();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::setDispatcher(std::weak_ptr<IDispatcher> pDispatcher) {
  return getImpl()->setDispatcher(pDispatcher);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::setDataCallback(std::weak_ptr<IDataCallback> pCallback) {
  return getImpl()->setDataCallback(pCallback);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<HalImageStreamBufferPoolT>
PipelineContext::queryImageStreamPool(StreamId_T const streamId) const {
  return getImpl()->queryImageStreamPool(streamId);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<INodeActor> PipelineContext::queryINodeActor(
    NodeId_T const nodeId) const {
  return getImpl()->queryNode(nodeId);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::queryStream(StreamId_T const streamId,
                             std::shared_ptr<IImageStreamInfo>* pStreamInfo) {
  std::shared_ptr<PipelineContextImpl> pImpl;
  pImpl.reset(this->getImpl());
  if (!pImpl.get()) {
    MY_LOGE("null contextimpl");
    return NAME_NOT_FOUND;
  }
  //
  std::shared_ptr<StreamConfig::ItemImageStream> pItem =
      pImpl->queryImage(streamId);
  if (!pItem.get() || !pItem->pInfo.get()) {
    MY_LOGD("no previous image stream");
    // this might success if both previous & new-coming pipeline have no this
    // stream.
    *pStreamInfo = nullptr;
    return OK;
  }
  //
  *pStreamInfo = pItem->pInfo;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::reuseStream(std::shared_ptr<IImageStreamInfo>* pStreamInfo) {
  if (!pStreamInfo->get() || !queryOldContext().get() ||
      !queryOldContext()->getImpl()) {
    return NAME_NOT_FOUND;
  }
  //
  MERROR err;
  PipelineContextImpl* pOldImpl = queryOldContext()->getImpl();
  StreamId_T const streamId = (*pStreamInfo)->getStreamId();
  std::shared_ptr<StreamConfig::ItemImageStream> pItem =
      pOldImpl->queryImage(streamId);
  if (!pItem.get()) {
    MY_LOGD("no previous stream");
    return BAD_VALUE;
  }
  if (OK != (err = getImpl()->reuseStream(pItem))) {
    return err;
  }
  *pStreamInfo = pItem->pInfo;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
PipelineContext::reuseNode(NodeId_T const nodeId) {
  if (!queryOldContext().get() || !queryOldContext()->getImpl()) {
    return NAME_NOT_FOUND;
  }
  //
  MERROR err;
  PipelineContextImpl* const pOldImpl = queryOldContext()->getImpl();
  //
  std::shared_ptr<NodeConfig> pOldNodeCfg = pOldImpl->getNodeConfig();
  if (!pOldNodeCfg.get()) {
    MY_LOGD("no previous node config");
    return BAD_VALUE;
  }
  std::shared_ptr<ContextNode> pNode = pOldNodeCfg->queryNode(nodeId);
  if (!pNode.get()) {
    MY_LOGD("no previous node context");
    return BAD_VALUE;
  }
  //
  StreamUsageMap const& usgMap = pOldNodeCfg->getImageStreamUsage(nodeId);
  if (OK != (err = getImpl()->reuseNode(nodeId, pNode, usgMap))) {
    return err;
  }
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineContext::dumpState(
    const std::vector<std::string>& options __unused) -> void {
  mpImpl->dumpState(options);
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineContext::dump() {
  mpImpl->dumpState({});
}

/******************************************************************************
 *
 ******************************************************************************/
uint32_t PipelineContext::getFrameNo() {
  return getImpl()->getFrameNo();
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
PipelineContext::setMultiCamSyncHelper(
    std::shared_ptr<MultiCamSyncHelper> const& helper) {
  MY_LOGD("set sync helper (%p)", helper.get());
  mpSyncHelper = helper;
}

/******************************************************************************
 *
 ******************************************************************************/
auto PipelineContext::getMultiCamSyncHelper()
    -> std::shared_ptr<MultiCamSyncHelper> {
  return mpSyncHelper;
}

/******************************************************************************
 *
 ******************************************************************************/
PipelineContext::PipelineContextImpl* PipelineContext::getImpl() const {
  return mpImpl.get();
}

/******************************************************************************
 *
 ******************************************************************************/
MUINT32
INodeActor::getStatus() const {
  std::lock_guard<std::mutex> _l(mLock);
  return muStatus;
}

/******************************************************************************
 *
 ******************************************************************************/
#define PRECHECK_STATUS(_status_, _skip_st_, _expected_)                     \
  do {                                                                       \
    if (_status_ >= _skip_st_) {                                             \
      MY_LOGD("%s already in state %d", getNode()->getNodeName(), _status_); \
      return OK;                                                             \
    } else if (_status_ != _expected_) {                                     \
      MY_LOGE("%s wrong status %d, expected %d", getNode()->getNodeName(),   \
              _status_, _expected_);                                         \
      return INVALID_OPERATION;                                              \
    }                                                                        \
  } while (0)

#define UPDATE_STATUS_IF_OK(_ret_, _status_var_, _newstatus_)  \
  do {                                                         \
    if (_ret_ == OK) {                                         \
      _status_var_ = _newstatus_;                              \
    } else {                                                   \
      MY_LOGE("%s ret = %d", getNode()->getNodeName(), _ret_); \
    }                                                          \
  } while (0)
/******************************************************************************
 *
 ******************************************************************************/
MERROR
INodeActor::init() {
  std::lock_guard<std::mutex> _l(mLock);
  PRECHECK_STATUS(muStatus, eNodeState_Init, eNodeState_Create);
  MERROR const ret = onInit();
  UPDATE_STATUS_IF_OK(ret, muStatus, eNodeState_Init);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
INodeActor::config() {
  std::lock_guard<std::mutex> _l(mLock);
  PRECHECK_STATUS(muStatus, eNodeState_Config, eNodeState_Init);
  MERROR const ret = onConfig();
  UPDATE_STATUS_IF_OK(ret, muStatus, eNodeState_Config);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
INodeActor::uninit() {
  std::lock_guard<std::mutex> _l(mLock);
  if (muStatus < eNodeState_Init) {
    MY_LOGD("already uninit or not init");
    return OK;
  }
  //
  MERROR const ret = onUninit();
  if (ret != OK) {
    MY_LOGE("uninit failed");
  }
  // always update
  muStatus = eNodeState_Create;
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
ContextNode::ContextNode(NodeId_T const nodeId,
                         std::shared_ptr<INodeActor> pNode)
    : mNodeId(nodeId),
      mpNode(pNode)
      //
      ,
      mpInStreams(nullptr),
      mpOutStreams(nullptr) {}

/******************************************************************************
 *
 ******************************************************************************/
ContextNode::~ContextNode() {}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
DispatcherBase::onEarlyCallback(MUINT32 requestNo,
                                Pipeline_NodeId_T nodeId,
                                StreamId_T streamId,
                                IMetadata const& rMetaData,
                                MBOOL errorResult) {
  std::shared_ptr<IDataCallback> pDataCallback = mpDataCallback.lock();
  if (pDataCallback.get()) {
    pDataCallback->onMetaCallback(requestNo, nodeId, streamId, rMetaData,
                                  errorResult);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
DispatcherBase::onCtrlSetting(MUINT32 requestNo,
                              Pipeline_NodeId_T nodeId,
                              StreamId_T const metaAppStreamId,
                              IMetadata const& rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata const& rHalMetaData,
                              MBOOL const& rIsChanged) {
  std::shared_ptr<IDataCallback> pDataCallback = mpDataCallback.lock();
  if (pDataCallback.get()) {
    if (pDataCallback->isCtrlSetting())
      pDataCallback->onCtrlSetting(requestNo, nodeId, metaAppStreamId,
                                   rAppMetaData, metaHalStreamId, rHalMetaData,
                                   rIsChanged);
    else
      MY_LOGD("NOT Support ControlCallback - Setting");
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
DispatcherBase::onCtrlSync(MUINT32 requestNo,
                           Pipeline_NodeId_T nodeId,
                           MUINT32 index,
                           MUINT32 type,
                           MINT64 duration) {
  std::shared_ptr<IDataCallback> pDataCallback = mpDataCallback.lock();
  if (pDataCallback.get()) {
    if (pDataCallback->isCtrlSync())
      pDataCallback->onCtrlSync(requestNo, nodeId, index, type, duration);
    else
      MY_LOGD("NOT Support ControlCallback - Sync");
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
DispatcherBase::onCtrlResize(MUINT32 requestNo,
                             Pipeline_NodeId_T nodeId,
                             StreamId_T const metaAppStreamId,
                             IMetadata const& rAppMetaData,
                             StreamId_T const metaHalStreamId,
                             IMetadata const& rHalMetaData,
                             MBOOL const& rIsChanged) {
  std::shared_ptr<IDataCallback> pDataCallback = mpDataCallback.lock();
  if (pDataCallback.get()) {
    if (pDataCallback->isCtrlResize())
      pDataCallback->onCtrlResize(requestNo, nodeId, metaAppStreamId,
                                  rAppMetaData, metaHalStreamId, rHalMetaData,
                                  rIsChanged);
    else
      MY_LOGD("NOT Support ControlCallback - Resize");
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
DispatcherBase::onCtrlReadout(MUINT32 requestNo,
                              Pipeline_NodeId_T nodeId,
                              StreamId_T const metaAppStreamId,
                              IMetadata const& rAppMetaData,
                              StreamId_T const metaHalStreamId,
                              IMetadata const& rHalMetaData,
                              MBOOL const& rIsChanged) {
  std::shared_ptr<IDataCallback> pDataCallback = mpDataCallback.lock();
  if (pDataCallback.get()) {
    if (pDataCallback->isCtrlSetting())
      pDataCallback->onCtrlSetting(requestNo, nodeId, metaAppStreamId,
                                   rAppMetaData, metaHalStreamId, rHalMetaData,
                                   rIsChanged);
    else
      MY_LOGD("NOT Support ControlCallback - Readout");
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
DispatcherBase::needCtrlCb(eCtrlType eType) {
  std::shared_ptr<IDataCallback> pDataCallback = mpDataCallback.lock();
  MBOOL res = MFALSE;
  if (pDataCallback.get()) {
    switch (eType) {
      case eCtrl_Setting:
        res = pDataCallback->isCtrlSetting();
        break;

      case eCtrl_Sync:
        res = pDataCallback->isCtrlSync();
        break;

      case eCtrl_Resize:
        res = pDataCallback->isCtrlResize();
        break;

      case eCtrl_Readout:
        res = pDataCallback->isCtrlReadout();
        break;

      default:
        break;
    }
  }
  return res;
}
/******************************************************************************
 *
 ******************************************************************************/
MVOID
DispatcherBase::onNextCaptureCallBack(MUINT32 requestNo,
                                      Pipeline_NodeId_T nodeId) {
  std::shared_ptr<IDataCallback> pDataCallback = mpDataCallback.lock();
  if (pDataCallback.get()) {
    pDataCallback->onNextCaptureCallBack(requestNo, nodeId);
  }
}
