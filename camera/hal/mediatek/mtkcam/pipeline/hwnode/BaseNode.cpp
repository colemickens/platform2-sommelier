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

#define LOG_TAG "MtkCam/BaseNode"
//
#include <memory>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Sync.h>
#include <property_service/property.h>
#include <property_service/property_lib.h>

#include "mtkcam/pipeline/hwnode/BaseNode.h"
//
// using namespace NSCam::Utils::Sync;

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::BaseNode::BaseNode() : mOpenId(-1L), mNodeId(NodeId_T(-1)) {
  char cLogLevel[PROPERTY_VALUE_MAX];
  property_get("vendor.debug.camera.log", cLogLevel, "0");
  mLogLevel = std::atoi(cLogLevel);
  if (0 == mLogLevel) {
    property_get("vendor.debug.camera.log.basenode", cLogLevel, "0");
    mLogLevel = ::atoi(cLogLevel);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
MINT32
NSCam::v3::BaseNode::getOpenId() const {
  return mOpenId;
}

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::IPipelineNode::NodeId_T NSCam::v3::BaseNode::getNodeId() const {
  return mNodeId;
}

/******************************************************************************
 *
 ******************************************************************************/
char const* NSCam::v3::BaseNode::getNodeName() const {
  return mNodeName.c_str();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::BaseNode::ensureMetaBufferAvailable(
    MUINT32 const frameNo,
    StreamId_T const streamId,
    IStreamBufferSet* rStreamBufferSet,
    std::shared_ptr<IMetaStreamBuffer>* rpStreamBuffer,
    MBOOL acquire) {
  FUNCTION_IN

  //  Ensure this buffer really comes with the request.
  //  A buffer may not exist due to partial requests.
  *rpStreamBuffer = rStreamBufferSet->getMetaBuffer(streamId, getNodeId());
  if (!*rpStreamBuffer) {
    MY_LOGD("[frame:%u node:%#" PRIxPTR "], streamID(%#" PRIx64 ")", frameNo,
            getNodeId(), streamId);
    return NAME_NOT_FOUND;
  }
  //
  if (acquire) {
    //
    //  Wait acquire_fence.
    std::shared_ptr<NSCam::Utils::Sync::IFence> acquire_fence =
        NSCam::Utils::Sync::IFence::create(
            (*rpStreamBuffer)->createAcquireFence(getNodeId()));
    MERROR const err = acquire_fence->waitForever(getNodeName());
    MY_LOGE_IF(
        OK != err,
        "[frame:%u node:%#" PRIxPTR
        "][stream buffer:%s] fail to wait acquire_fence:%d[%s] err:%d[%s]",
        frameNo, getNodeId(), (*rpStreamBuffer)->getName(),
        acquire_fence->getFd(), acquire_fence->name(), err, ::strerror(-err));
    //
    //  Mark this buffer as ACQUIRED by this user.
    rStreamBufferSet->markUserStatus(streamId, getNodeId(),
                                     IUsersManager::UserStatus::ACQUIRE);
  }
  //
  //  Check buffer status.
  if ((*rpStreamBuffer)->hasStatus(STREAM_BUFFER_STATUS::ERROR)) {
    MY_LOGE("[frame:%u node:%#" PRIxPTR "][stream buffer:%s] bad status:%d",
            frameNo, getNodeId(), (*rpStreamBuffer)->getName(),
            (*rpStreamBuffer)->getStatus());
    *rpStreamBuffer = nullptr;
    return BAD_VALUE;
  }

  //
  FUNCTION_OUT

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::BaseNode::ensureImageBufferAvailable(
    MUINT32 const frameNo,
    StreamId_T const streamId,
    IStreamBufferSet* rStreamBufferSet,
    std::shared_ptr<IImageStreamBuffer>* rpStreamBuffer,
    MBOOL acquire) {
  FUNCTION_IN

  //  Ensure this buffer really comes with the request.
  //  A buffer may not exist due to partial requests.
  *rpStreamBuffer = rStreamBufferSet->getImageBuffer(streamId, getNodeId());
  if (!(*rpStreamBuffer)) {
    MY_LOGD("[frame:%d node:%#" PRIxPTR "], streamID(%#" PRIx64 ")", frameNo,
            getNodeId(), streamId);
    return NAME_NOT_FOUND;
  }
  //
  if (acquire) {
    //
    //  Wait acquire_fence.
    std::shared_ptr<NSCam::Utils::Sync::IFence> acquire_fence =
        NSCam::Utils::Sync::IFence::create(
            (*rpStreamBuffer)->createAcquireFence(getNodeId()));
    MERROR const err = acquire_fence->waitForever(getNodeName());
    MY_LOGE_IF(
        OK != err,
        "[frame:%d node:%#" PRIxPTR
        "][stream buffer:%s] fail to wait acquire_fence:%d[%s] err:%d[%s]",
        frameNo, getNodeId(), (*rpStreamBuffer)->getName(),
        acquire_fence->getFd(), acquire_fence->name(), err, ::strerror(-err));
    //
    //  Mark this buffer as ACQUIRED by this user.
    rStreamBufferSet->markUserStatus(streamId, getNodeId(),
                                     IUsersManager::UserStatus::ACQUIRE);
  }
  //
  //  Check buffer status.
  if ((*rpStreamBuffer)->hasStatus(STREAM_BUFFER_STATUS::ERROR)) {
    //  The producer ahead of this user may fail to render this buffer's
    //  content.
    MY_LOGE("[frame:%u node:%#" PRIxPTR "][stream buffer:%s] bad status:%d",
            frameNo, getNodeId(), (*rpStreamBuffer)->getName(),
            (*rpStreamBuffer)->getStatus());
    (*rpStreamBuffer) = nullptr;
    return BAD_VALUE;
  }

  FUNCTION_OUT
  //
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::BaseNode::onDispatchFrame(
    std::shared_ptr<IPipelineFrame> const& pFrame) {
  FUNCTION_IN

  std::shared_ptr<IPipelineNodeCallback> cb = pFrame->getPipelineNodeCallback();
  if (cb != nullptr) {
    cb->onDispatchFrame(pFrame, getNodeId());
  }

  FUNCTION_OUT
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::BaseNode::onEarlyCallback(
    std::shared_ptr<IPipelineFrame> const& pFrame,
    StreamId_T const streamId,
    IMetadata const& rMetaData,
    MBOOL error) {
  FUNCTION_IN

  std::shared_ptr<IPipelineNodeCallback> cb = pFrame->getPipelineNodeCallback();
  if (cb != nullptr) {
    cb->onEarlyCallback(pFrame->getRequestNo(), getNodeId(), streamId,
                        rMetaData, error);
  }

  FUNCTION_OUT
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::BaseNode::onCtrlSetting(
    std::shared_ptr<IPipelineFrame> const& pFrame,
    StreamId_T const metaAppStreamId,
    IMetadata* rAppMetaData,
    StreamId_T const metaHalStreamId,
    IMetadata* rHalMetaData,
    MBOOL* rIsChanged) {
  FUNCTION_IN

  std::shared_ptr<IPipelineNodeCallback> cb = pFrame->getPipelineNodeCallback();
  if (cb != nullptr && cb->needCtrlCb(IPipelineNodeCallback::eCtrl_Setting)) {
    cb->onCtrlSetting(pFrame->getRequestNo(), getNodeId(), metaAppStreamId,
                      *rAppMetaData, metaHalStreamId, *rHalMetaData,
                      *rIsChanged);
  }

  FUNCTION_OUT
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::BaseNode::onCtrlSync(std::shared_ptr<IPipelineFrame> const& pFrame,
                                MUINT32 index,
                                MUINT32 type,
                                MINT64 duration) {
  FUNCTION_IN

  std::shared_ptr<IPipelineNodeCallback> cb = pFrame->getPipelineNodeCallback();
  if (cb != nullptr && cb->needCtrlCb(IPipelineNodeCallback::eCtrl_Sync)) {
    cb->onCtrlSync(pFrame->getRequestNo(), getNodeId(), index, type, duration);
  }

  FUNCTION_OUT
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::BaseNode::onCtrlResize(std::shared_ptr<IPipelineFrame> const& pFrame,
                                  StreamId_T const metaAppStreamId,
                                  IMetadata* rAppMetaData,
                                  StreamId_T const metaHalStreamId,
                                  IMetadata* rHalMetaData,
                                  MBOOL* rIsChanged) {
  FUNCTION_IN

  std::shared_ptr<IPipelineNodeCallback> cb = pFrame->getPipelineNodeCallback();
  if (cb != nullptr && cb->needCtrlCb(IPipelineNodeCallback::eCtrl_Resize)) {
    cb->onCtrlResize(pFrame->getRequestNo(), getNodeId(), metaAppStreamId,
                     *rAppMetaData, metaHalStreamId, *rHalMetaData,
                     *rIsChanged);
  }

  FUNCTION_OUT
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::BaseNode::onCtrlReadout(
    std::shared_ptr<IPipelineFrame> const& pFrame,
    StreamId_T const metaAppStreamId,
    IMetadata* rAppMetaData,
    StreamId_T const metaHalStreamId,
    IMetadata* rHalMetaData,
    MBOOL* rIsChanged) {
  FUNCTION_IN

  std::shared_ptr<IPipelineNodeCallback> cb = pFrame->getPipelineNodeCallback();
  if (cb != nullptr && cb->needCtrlCb(IPipelineNodeCallback::eCtrl_Readout)) {
    cb->onCtrlReadout(pFrame->getRequestNo(), getNodeId(), metaAppStreamId,
                      *rAppMetaData, metaHalStreamId, *rHalMetaData,
                      *rIsChanged);
  }

  FUNCTION_OUT
}

/******************************************************************************
 *
 ******************************************************************************/
MBOOL
NSCam::v3::BaseNode::needCtrlCb(std::shared_ptr<IPipelineFrame> const& pFrame,
                                IPipelineNodeCallback::eCtrlType eType) {
  FUNCTION_IN

  MBOOL en = MFALSE;
  std::shared_ptr<IPipelineNodeCallback> cb = pFrame->getPipelineNodeCallback();
  if (cb != nullptr) {
    en = cb->needCtrlCb(eType);
  }

  FUNCTION_OUT
  return en;
}

/******************************************************************************
 *
 ******************************************************************************/
MVOID
NSCam::v3::BaseNode::onNextCaptureCallBack(
    std::shared_ptr<IPipelineFrame> const& pFrame) {
  FUNCTION_IN

  std::shared_ptr<IPipelineNodeCallback> cb = pFrame->getPipelineNodeCallback();
  if (cb != nullptr) {
    cb->onNextCaptureCallBack(pFrame->getRequestNo(), getNodeId());
  }

  FUNCTION_OUT
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::BaseNode::setNodeCallBack(
    std::weak_ptr<INodeCallbackToPipeline> pCallback) {
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::BaseNode::kick() {
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
NSCam::v3::BaseNode::flush(std::shared_ptr<IPipelineFrame> const& pFrame) {
  FUNCTION_IN;
  //
  MERROR err = OK;

  ////////////////////////////////////////////////////////////////////////////
  //
  ////////////////////////////////////////////////////////////////////////////
  //
  //  Note:
  //  1. Don't mark ACQUIRE if never waiting on its acquire fence.
  //  2. Don't mark IN_FLIGHT (so we know the producer has not touched the
  //  buffer, and its content is ERROR).
  //  3. (Producer) users Needn't set its buffer status to ERROR.

  ////////////////////////////////////////////////////////////////////////////
  //  Mark buffers as RELEASE.
  ////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<IStreamInfoSet const> pIStreams, pOStreams;
  IStreamBufferSet& rStreamBufferSet = pFrame->getStreamBufferSet();
  //
  err = pFrame->queryIOStreamInfoSet(getNodeId(), &pIStreams, &pOStreams);
  if (OK != err) {
    MY_LOGE("nodeId:%#" PRIxPTR " frameNo:%d queryIOStreamInfoSet", getNodeId(),
            pFrame->getFrameNo());
  }
  //
  if (IStreamInfoSet const* pStreams = pIStreams.get()) {
    {
      // I:Meta
      std::shared_ptr<IStreamInfoSet::IMap<IMetaStreamInfo> > pMap =
          pStreams->getMetaInfoMap();
      for (size_t i = 0; i < pMap->size(); i++) {
        StreamId_T const streamId = pMap->valueAt(i)->getStreamId();
        //  Mark this buffer as RELEASE by this user.
        std::shared_ptr<IStreamBuffer> pStreamBuffer =
            rStreamBufferSet.getMetaBuffer(streamId, getNodeId());
        if (pStreamBuffer) {
          pStreamBuffer->markUserStatus(getNodeId(),
                                        IUsersManager::UserStatus::RELEASE);
        }
      }
    }
    {
      // I:Image
      std::shared_ptr<IStreamInfoSet::IMap<IImageStreamInfo> > pMap =
          pStreams->getImageInfoMap();
      for (size_t i = 0; i < pMap->size(); i++) {
        StreamId_T const streamId = pMap->valueAt(i)->getStreamId();
        //  Mark this buffer as RELEASE by this user.
        std::shared_ptr<IStreamBuffer> pStreamBuffer =
            rStreamBufferSet.getImageBuffer(streamId, getNodeId());
        if (pStreamBuffer) {
          pStreamBuffer->markUserStatus(getNodeId(),
                                        IUsersManager::UserStatus::RELEASE);
        }
      }
    }
  } else {
    MY_LOGE("nodeId:%#" PRIxPTR " frameNo:%d NULL IStreams", getNodeId(),
            pFrame->getFrameNo());
  }
  //
  if (IStreamInfoSet const* pStreams = pOStreams.get()) {
    {
      // O:Meta
      std::shared_ptr<IStreamInfoSet::IMap<IMetaStreamInfo> > pMap =
          pStreams->getMetaInfoMap();
      for (size_t i = 0; i < pMap->size(); i++) {
        StreamId_T const streamId = pMap->valueAt(i)->getStreamId();
        //  Mark this buffer as RELEASE by this user.
        std::shared_ptr<IStreamBuffer> pStreamBuffer =
            rStreamBufferSet.getMetaBuffer(streamId, getNodeId());
        if (pStreamBuffer) {
          pStreamBuffer->markUserStatus(getNodeId(),
                                        IUsersManager::UserStatus::RELEASE);
        }
      }
    }
    {
      // O:Image
      std::shared_ptr<IStreamInfoSet::IMap<IImageStreamInfo> > pMap =
          pStreams->getImageInfoMap();
      for (size_t i = 0; i < pMap->size(); i++) {
        StreamId_T const streamId = pMap->valueAt(i)->getStreamId();
        //  Mark this buffer as RELEASE by this user.
        std::shared_ptr<IStreamBuffer> pStreamBuffer =
            rStreamBufferSet.getImageBuffer(streamId, getNodeId());
        if (pStreamBuffer) {
          pStreamBuffer->markUserStatus(getNodeId(),
                                        IUsersManager::UserStatus::RELEASE);
        }
      }
    }
  } else {
    MY_LOGE("nodeId:%#" PRIxPTR " frameNo:%d NULL OStreams", getNodeId(),
            pFrame->getFrameNo());
  }

  ////////////////////////////////////////////////////////////////////////////
  //  Apply buffers to release.
  ////////////////////////////////////////////////////////////////////////////
  rStreamBufferSet.applyRelease(getNodeId());

  ////////////////////////////////////////////////////////////////////////////
  //  Dispatch
  ////////////////////////////////////////////////////////////////////////////
  onDispatchFrame(pFrame);
  //
  FUNCTION_OUT;
  return OK;
}
