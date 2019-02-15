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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_MWFrame.h"

#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG MWFrame
#define P2_TRACE TRACE_MW_FRAME
#include "P2_LogHeader.h"

#include <memory>
#include <string>

namespace P2 {

MWFrame::MWFrame(const ILog& log,
                 const IPipelineNode::NodeId_T& nodeID,
                 const NodeName_T& nodeName,
                 const std::shared_ptr<IPipelineFrame>& frame)
    : mLog(log),
      mNodeID(nodeID),
      mNodeName(nodeName),
      mFrame(frame),
      mDirty(MFALSE),
      mBatchMode(0) {
  TRACE_S_FUNC_ENTER(mLog);
  mTraceName = base::StringPrintf("Cam:%d:IspP2|%d|request:%d frame:%d",
                                  mLog.getLogSensorID(), mFrame->getRequestNo(),
                                  mFrame->getRequestNo(), mFrame->getFrameNo());
  if (ATRACE_ENABLED()) {
    P2_CAM_TRACE_ASYNC_BEGIN(TRACE_DEFAULT, mTraceName.c_str(), 0);
  }
  P2_CAM_TRACE_ASYNC_BEGIN(TRACE_ADVANCED, "P2_MWFrame", mFrame->getFrameNo());
  TRACE_S_FUNC_EXIT(mLog);
}

MWFrame::~MWFrame() {
  TRACE_S_FUNC_ENTER(mLog);
  if (mDirty) {
    doRelease();
  }
  dispatchFrame(mLog, mFrame, mNodeID);
  if (ATRACE_ENABLED()) {
    P2_CAM_TRACE_ASYNC_END(TRACE_DEFAULT, mTraceName.c_str(), 0);
  }
  P2_CAM_TRACE_ASYNC_END(TRACE_ADVANCED, "P2_MWFrame", mFrame->getFrameNo());
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrame::dispatchFrame(const ILog& log,
                             const std::shared_ptr<IPipelineFrame>& frame,
                             const IPipelineNode::NodeId_T& nodeID) {
  (void)log;
  TRACE_S_FUNC_ENTER(log);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2:DispatchFrame");
  std::shared_ptr<IPipelineNodeCallback> cb;
  if (frame != nullptr) {
    cb = frame->getPipelineNodeCallback();
    if (cb != nullptr) {
      cb->onDispatchFrame(frame, nodeID);
    }
  }
  TRACE_S_FUNC_EXIT(log);
}

MVOID releaseStream(const ILog& log,
                    const IPipelineNode::NodeId_T& nodeID,
                    IStreamBufferSet* streamBufferSet,
                    const std::shared_ptr<const IStreamInfoSet>& stream) {
  (void)log;
  TRACE_S_FUNC_ENTER(log);
  std::shared_ptr<IStreamInfoSet::IMap<IMetaStreamInfo>> meta;
  std::shared_ptr<IStreamInfoSet::IMap<IImageStreamInfo>> img;
  if (stream != nullptr) {
    meta = stream->getMetaInfoMap();
    img = stream->getImageInfoMap();
  }
  for (unsigned i = 0, n = (meta != nullptr ? meta->size() : 0); i < n; ++i) {
    StreamId_T sID = meta->valueAt(i)->getStreamId();
    std::shared_ptr<IStreamBuffer> buffer =
        streamBufferSet->getMetaBuffer(sID, nodeID);
    if (buffer != nullptr) {
      buffer->markUserStatus(nodeID, IUsersManager::UserStatus::RELEASE);
    }
  }
  for (unsigned i = 0, n = (img != nullptr ? img->size() : 0); i < n; ++i) {
    StreamId_T sID = img->valueAt(i)->getStreamId();
    std::shared_ptr<IStreamBuffer> buffer =
        streamBufferSet->getImageBuffer(sID, nodeID);
    if (buffer != nullptr) {
      buffer->markUserStatus(nodeID, IUsersManager::UserStatus::RELEASE);
    }
  }
  streamBufferSet->applyRelease(nodeID);
  TRACE_S_FUNC_EXIT(log);
}

MVOID MWFrame::releaseFrameStream(const ILog& log,
                                  const std::shared_ptr<IPipelineFrame>& frame,
                                  const IPipelineNode::NodeId_T& nodeID) {
  TRACE_S_FUNC_ENTER(log);
  std::shared_ptr<const IStreamInfoSet> iStream, oStream;
  if (frame == nullptr) {
    MY_S_LOGW(log, "invalid frame = nullptr");
  } else if (0 != frame->queryIOStreamInfoSet(nodeID, &iStream, &oStream)) {
    MY_S_LOGW(log, "queryIOStreamInfoSet failed");
  } else {
    MY_S_LOGI(log, "Node%#" PRIxPTR " Flush FrameNo(%d)", nodeID,
              frame->getFrameNo());
    IStreamBufferSet& streamBufferSet = frame->getStreamBufferSet();
    releaseStream(log, nodeID, &streamBufferSet, iStream);
    releaseStream(log, nodeID, &streamBufferSet, oStream);
  }
  TRACE_S_FUNC_EXIT(log);
}

MVOID MWFrame::flushFrame(const ILog& log,
                          const std::shared_ptr<IPipelineFrame>& frame,
                          const IPipelineNode::NodeId_T& nodeID) {
  TRACE_S_FUNC_ENTER(log);
  if (frame == nullptr) {
    MY_S_LOGW(log, "invalid frame = NULL");
  } else {
    MWFrame::releaseFrameStream(log, frame, nodeID);
    MWFrame::dispatchFrame(log, frame, nodeID);
  }
  TRACE_S_FUNC_EXIT(log);
}

MUINT32 MWFrame::getMWFrameID() const {
  TRACE_S_FUNC_ENTER(mLog);
  MUINT32 id = 0;
  if (mFrame != nullptr) {
    id = mFrame->getFrameNo();
  }
  TRACE_S_FUNC_EXIT(mLog);
  return id;
}

MUINT32 MWFrame::getMWFrameRequestID() const {
  TRACE_S_FUNC_ENTER(mLog);
  MUINT32 id = 0;
  if (mFrame != nullptr) {
    id = mFrame->getRequestNo();
  }
  TRACE_S_FUNC_EXIT(mLog);
  return id;
}

MUINT32 MWFrame::getFrameID() const {
  TRACE_S_FUNC_ENTER(mLog);
  TRACE_S_FUNC_EXIT(mLog);
  return mLog.getLogFrameID();
}

MVOID MWFrame::notifyRelease() {
  TRACE_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  if (!mBatchMode) {
    doRelease();
  } else {
    mDirty = MTRUE;
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrame::beginBatchRelease() {
  TRACE_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  ++mBatchMode;
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrame::endBatchRelease() {
  TRACE_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  --mBatchMode;
  if (!mBatchMode && mDirty) {
    doRelease();
    mDirty = MFALSE;
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrame::notifyNextCapture() {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<IPipelineNodeCallback> cb = mFrame->getPipelineNodeCallback();
  if (cb != nullptr) {
    cb->onNextCaptureCallBack(mFrame->getRequestNo(), mNodeID);
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MBOOL MWFrame::getInfoIOMapSet(IPipelineFrame::InfoIOMapSet* ioMap) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  ret = (0 == mFrame->queryInfoIOMapSet(mNodeID, ioMap));
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

std::shared_ptr<IMetaStreamBuffer> MWFrame::acquireMetaStream(
    const StreamId_T& sID) {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<IMetaStreamBuffer> streamBuffer;
  IStreamBufferSet& bufferSet = mFrame->getStreamBufferSet();
  streamBuffer = bufferSet.getMetaBuffer(sID, mNodeID);
  if (!validateStream(sID, &bufferSet, streamBuffer)) {
    streamBuffer = nullptr;
  } else {
    mMWStreamMap[sID] =
        MWStream(streamBuffer->getStreamInfo()->getStreamName(), STATE_USING);
  }
  TRACE_S_FUNC_EXIT(mLog);
  return streamBuffer;
}

std::shared_ptr<IImageStreamBuffer> MWFrame::acquireImageStream(
    const StreamId_T& sID) {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<IImageStreamBuffer> streamBuffer;
  IStreamBufferSet& bufferSet = mFrame->getStreamBufferSet();
  streamBuffer = bufferSet.getImageBuffer(sID, mNodeID);
  if (!validateStream(sID, &bufferSet, streamBuffer)) {
    streamBuffer = nullptr;
  } else {
    mMWStreamMap[sID] =
        MWStream(streamBuffer->getStreamInfo()->getStreamName(), STATE_USING);
  }
  TRACE_S_FUNC_EXIT(mLog);
  return streamBuffer;
}

IMetadata* MWFrame::acquireMeta(
    const std::shared_ptr<IMetaStreamBuffer>& stream, IO_DIR dir) const {
  TRACE_S_FUNC_ENTER(mLog);
  IMetadata* meta = nullptr;
  if (stream != nullptr) {
    meta = (dir & IO_DIR_OUT) ? stream->tryWriteLock(mNodeName.c_str())
                              : stream->tryReadLock(mNodeName.c_str());
    if (meta == nullptr) {
      MY_S_LOGW(mLog, "(%s)metaStreamBuffer->tryLock() failed",
                stream->getName());
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return meta;
}

std::shared_ptr<IImageBuffer> MWFrame::acquireImage(
    const std::shared_ptr<IImageStreamBuffer>& stream,
    IO_DIR dir,
    MBOOL needSWRW) const {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<IImageBuffer> image = nullptr;
  std::shared_ptr<IImageBufferHeap> heap = nullptr;
  if (stream != nullptr) {
    if ((dir & IO_DIR_OUT)) {
      heap.reset(stream->tryWriteLock(mNodeName.c_str()),
                 [](IImageBufferHeap* p) { MY_LOGI("release implement"); });
    } else {
      heap.reset(stream->tryReadLock(mNodeName.c_str()),
                 [](IImageBufferHeap* p) { MY_LOGI("release implement"); });
    }
    if (heap == nullptr) {
      MY_S_LOGW(mLog, "(%s)imageStreamBuffer->tryLock() failed",
                stream->getName());
    } else {
      image = heap->createImageBuffer();
      if (image == nullptr) {
        MY_S_LOGW(mLog, "(%s) heap->createImageBuffer() failed",
                  stream->getName());
      } else {
        MUINT32 usage = stream->queryGroupUsage(mNodeID);
        if (needSWRW && (dir & IO_DIR_OUT)) {
          usage |= NSCam::eBUFFER_USAGE_SW_MASK;
        }
        if (!image->lockBuf(mNodeName.c_str(), usage)) {
          MY_S_LOGW(mLog, "(%s) image buffer lock usage(0x%x) failed",
                    stream->getName(), usage);
          releaseImage(stream, image);
          image = nullptr;
        }
      }
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return image;
}

std::shared_ptr<IImageBuffer> MWFrame::acquireOpaqueImage(
    const std::shared_ptr<IImageStreamBuffer>& stream, IO_DIR dir) const {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<IImageBuffer> image = nullptr;
  if (stream != nullptr) {
    std::shared_ptr<IImageBufferHeap> heap = nullptr;
    if ((dir & IO_DIR_OUT)) {
      heap.reset(stream->tryWriteLock(mNodeName.c_str()),
                 [](IImageBufferHeap* p) { MY_LOGI("release implement"); });
    } else {
      heap.reset(stream->tryReadLock(mNodeName.c_str()),
                 [](IImageBufferHeap* p) { MY_LOGI("release implement"); });
    }
    if (heap == nullptr) {
      MY_S_LOGW(mLog, "streamBuffer->tryLock() failed");
    } else {
      heap->lockBuf(mNodeName.c_str());
      OpaqueReprocUtil::getImageBufferFromHeap(heap, &image);
      heap->unlockBuf(mNodeName.c_str());
      if (image == nullptr) {
        MY_S_LOGW(mLog, "heap->createImageBuffer() failed");
      } else {
        MUINT32 usage = stream->queryGroupUsage(mNodeID);
        image->lockBuf(mNodeName.c_str(), usage);
      }
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return image;
}

MVOID MWFrame::releaseMeta(const std::shared_ptr<IMetaStreamBuffer>& stream,
                           IMetadata* meta) const {
  TRACE_S_FUNC_ENTER(mLog);
  if (stream != nullptr && meta) {
    stream->unlock(mNodeName.c_str(), meta);
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrame::releaseMetaStream(
    const std::shared_ptr<IMetaStreamBuffer>& stream,
    IO_DIR dir,
    IO_STATUS status) {
  TRACE_S_FUNC_ENTER(mLog);
  if (stream != nullptr) {
    if (dir & IO_DIR_OUT) {
      stream->markStatus(toStreamBufferStatus(status));
    }
    StreamId_T streamID = stream->getStreamInfo()->getStreamId();
    mMWStreamMap[streamID].mState = STATE_RELEASING;

    mFrame->getStreamBufferSet().markUserStatus(streamID, mNodeID,
                                                toUserStatus(status));
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrame::releaseImage(const std::shared_ptr<IImageStreamBuffer>& stream,
                            const std::shared_ptr<IImageBuffer>& image) const {
  TRACE_S_FUNC_ENTER(mLog);
  if (stream != nullptr && image != nullptr) {
    image->unlockBuf(mNodeName.c_str());
    stream->unlock(mNodeName.c_str(), image->getImageBufferHeap().get());
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrame::releaseImageStream(
    const std::shared_ptr<IImageStreamBuffer>& stream,
    IO_DIR dir,
    IO_STATUS status) {
  TRACE_S_FUNC_ENTER(mLog);
  if (stream != nullptr) {
    if (dir & IO_DIR_OUT) {
      stream->markStatus(toStreamBufferStatus(status));
    }
    StreamId_T streamID = stream->getStreamInfo()->getStreamId();
    mMWStreamMap[streamID].mState = STATE_RELEASING;
    mFrame->getStreamBufferSet().markUserStatus(streamID, mNodeID,
                                                toUserStatus(status));
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrame::print(const ILog& log,
                     const std::shared_ptr<IMetaStreamInfo>& info,
                     StreamId_T id,
                     unsigned s,
                     unsigned i,
                     const char* io) {
  const char* name = "NA";
  if (info != nullptr) {
    name = info->getStreamName();
  }
  MY_S_LOGD(log, "StreamInfo: metaSet[%d].%s[%d: 0x%09" PRIx64 "] %s", s, io, i,
            id, name);
}

MVOID MWFrame::print(const ILog& log,
                     const std::shared_ptr<IImageStreamInfo>& info,
                     StreamId_T id,
                     unsigned s,
                     unsigned i,
                     const char* io) {
  const char* name = "NA";
  MINT imgFmt = 0;
  MSize imgSize;
  if (info != nullptr) {
    name = info->getStreamName();
    imgFmt = info->getImgFormat();
    imgSize = info->getImgSize();
  }
  MY_S_LOGD(log,
            "StreamInfo: imgSet[%d].%s[%d: 0x%09" PRIx64
            "] %s (%dx%d) (fmt:0x%08x)",
            s, io, i, id, name, imgSize.w, imgSize.h, imgFmt);
}

MVOID MWFrame::print(const ILog& log,
                     const IPipelineFrame::InfoIOMapSet& ioMap) {
  const IPipelineFrame::ImageInfoIOMapSet& imgs = ioMap.mImageInfoIOMapSet;
  const IPipelineFrame::MetaInfoIOMapSet& metas = ioMap.mMetaInfoIOMapSet;

  unsigned index = 0;
  for (unsigned i = 0, size = imgs.size(); i < size; ++i) {
    index = 0;
    for (auto in = imgs[i].vIn.begin(); in != imgs[i].vIn.end();
         ++in, index++) {
      print(log, in->second, in->first, i, index, "in");
    }
    index = 0;
    for (auto out = imgs[i].vOut.begin(); out != imgs[i].vOut.end();
         ++out, index++) {
      print(log, out->second, out->first, i, index, "out");
    }
  }

  for (unsigned i = 0, size = metas.size(); i < size; ++i) {
    index = 0;
    for (auto in = metas[i].vIn.begin(); in != metas[i].vIn.end();
         ++in, index++) {
      print(log, in->second, in->first, i, index, "in");
    }
    index = 0;
    for (auto out = metas[i].vOut.begin(); out != metas[i].vOut.end();
         ++out, index++) {
      print(log, out->second, out->first, i, index, "out");
    }
  }
}

MVOID MWFrame::doRelease() {
  TRACE_S_FUNC_ENTER(mLog);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "P2:ApplyRelease");
  std::string streamStatus;
  for (auto&& it : mMWStreamMap) {
    base::StringAppendF(&streamStatus, "%s(%u),", it.second.mName.c_str(),
                        it.second.mState);
    if (it.second.mState == STATE_RELEASING) {
      it.second.mState = STATE_RELEASED;
    }
  }
  MY_S_LOGD(mLog, "all streams(%zu) status: %s", mMWStreamMap.size(),
            streamStatus.c_str());
  mFrame->getStreamBufferSet().applyRelease(mNodeID);
  TRACE_S_FUNC_EXIT(mLog);
}

MVOID MWFrame::acquireFence(
    const std::shared_ptr<IStreamBuffer>& stream) const {
  TRACE_S_FUNC_ENTER(mLog);
  std::shared_ptr<IFence> fence =
      IFence::create(stream->createAcquireFence(mNodeID));
  NSCam::MERROR ret = fence->waitForever(mNodeName.c_str());
  if (ret != 0) {
    MY_S_LOGE(
        mLog,
        "acquireFence->waitForever() failed buffer:%s fence:%d[%s] err:%d[%s]",
        stream->getName(), fence->getFd(), fence->name(), ret,
        ::strerror(-ret));
  }
  TRACE_S_FUNC_EXIT(mLog);
}

MBOOL MWFrame::validateStream(const StreamId_T& sID,
                              IStreamBufferSet* bufferSet,
                              const std::shared_ptr<IStreamBuffer>& stream,
                              MBOOL acquire) const {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MTRUE;
  if (stream == nullptr) {
    MY_S_LOGD(mLog, "stream [0x%09" PRIx64 "] getStreamBuffer() failed", sID);
    ret = MFALSE;
  } else {
    if (acquire) {
      acquireFence(stream);
      bufferSet->markUserStatus(sID, mNodeID,
                                IUsersManager::UserStatus::ACQUIRE);
    }
    if (stream->hasStatus(STREAM_BUFFER_STATUS::ERROR)) {
      MY_S_LOGW(mLog, "stream buffer:%s bad status:%u", stream->getName(),
                stream->getStatus());
      bufferSet->markUserStatus(sID, mNodeID,
                                IUsersManager::UserStatus::RELEASE);
      ret = MFALSE;
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MUINT32 MWFrame::toStreamBufferStatus(IO_STATUS status) const {
  TRACE_S_FUNC_ENTER(mLog);
  TRACE_S_FUNC_EXIT(mLog);
  return (status == IO_STATUS_OK) ? STREAM_BUFFER_STATUS::WRITE_OK
                                  : STREAM_BUFFER_STATUS::WRITE_ERROR;
}

MUINT32 MWFrame::toUserStatus(IO_STATUS status) const {
  TRACE_S_FUNC_ENTER(mLog);
  MUINT32 userStatus = IUsersManager::UserStatus::RELEASE;
  userStatus |=
      (status == IO_STATUS_INVALID) ? 0 : IUsersManager::UserStatus::USED;
  TRACE_S_FUNC_EXIT(mLog);
  return userStatus;
}

}  // namespace P2
