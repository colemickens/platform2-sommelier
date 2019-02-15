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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_MWFRAME_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_MWFRAME_H_

#include "P2_Param.h"
#include "P2_Request.h"

#include <map>
#include <memory>
#include <string>

namespace P2 {

typedef std::string NodeName_T;

#define USE_ACQUIRE MTRUE
class MWFrame : virtual public IP2Frame {
 public:
  MWFrame(const ILog& log,
          const IPipelineNode::NodeId_T& nodeID,
          const NodeName_T& nodeName,
          const std::shared_ptr<IPipelineFrame>& frame);
  virtual ~MWFrame();
  static MVOID dispatchFrame(const ILog& log,
                             const std::shared_ptr<IPipelineFrame>& frame,
                             const IPipelineNode::NodeId_T& nodeID);
  static MVOID releaseFrameStream(const ILog& log,
                                  const std::shared_ptr<IPipelineFrame>& frame,
                                  const IPipelineNode::NodeId_T& nodeID);
  static MVOID flushFrame(const ILog& log,
                          const std::shared_ptr<IPipelineFrame>& frame,
                          const IPipelineNode::NodeId_T& nodeID);
  MUINT32 getMWFrameID() const;
  MUINT32 getMWFrameRequestID() const;
  MUINT32 getFrameID() const;
  MVOID notifyRelease();
  MVOID beginBatchRelease();
  MVOID endBatchRelease();
  MVOID notifyNextCapture();
  MBOOL getInfoIOMapSet(IPipelineFrame::InfoIOMapSet* ioMap);
  std::shared_ptr<IMetaStreamBuffer> acquireMetaStream(const StreamId_T& sID);
  std::shared_ptr<IImageStreamBuffer> acquireImageStream(const StreamId_T& sID);
  IMetadata* acquireMeta(const std::shared_ptr<IMetaStreamBuffer>& stream,
                         IO_DIR dir) const;
  std::shared_ptr<IImageBuffer> acquireImage(
      const std::shared_ptr<IImageStreamBuffer>& stream,
      IO_DIR dir,
      MBOOL needSWRW) const;
  std::shared_ptr<IImageBuffer> acquireOpaqueImage(
      const std::shared_ptr<IImageStreamBuffer>& stream, IO_DIR dir) const;
  MVOID releaseMeta(const std::shared_ptr<IMetaStreamBuffer>& stream,
                    IMetadata* meta) const;
  MVOID releaseMetaStream(const std::shared_ptr<IMetaStreamBuffer>& stream,
                          IO_DIR dir,
                          IO_STATUS state);
  MVOID releaseImage(const std::shared_ptr<IImageStreamBuffer>& stream,
                     const std::shared_ptr<IImageBuffer>& image) const;
  MVOID releaseImageStream(const std::shared_ptr<IImageStreamBuffer>& stream,
                           IO_DIR dir,
                           IO_STATUS state);

  static MVOID print(const ILog& log,
                     const std::shared_ptr<IMetaStreamInfo>& info,
                     StreamId_T id,
                     unsigned s,
                     unsigned i,
                     const char* io);
  static MVOID print(const ILog& log,
                     const std::shared_ptr<IImageStreamInfo>& info,
                     StreamId_T id,
                     unsigned s,
                     unsigned i,
                     const char* io);
  static MVOID print(const ILog& log,
                     const IPipelineFrame::InfoIOMapSet& ioMap);

 private:
  MVOID doRelease();
  MVOID acquireFence(const std::shared_ptr<IStreamBuffer>& stream) const;
  MBOOL validateStream(const StreamId_T& sID,
                       IStreamBufferSet* bufferSet,
                       const std::shared_ptr<IStreamBuffer>& stream,
                       MBOOL acquire = USE_ACQUIRE) const;
  MUINT32 toStreamBufferStatus(IO_STATUS status) const;
  MUINT32 toUserStatus(IO_STATUS status) const;

 private:
  enum StreamState {
    STATE_RELEASED = 0,
    STATE_RELEASING,
    STATE_USING,
  };

  class MWStream {
   public:
    std::string mName;
    MUINT8 mState = STATE_USING;

    MWStream(const char* name, StreamState state)
        : mName(name), mState(state) {}

    MWStream() {}
  };

  std::mutex mMutex;
  ILog mLog;
  IPipelineNode::NodeId_T mNodeID;
  const NodeName_T mNodeName;
  std::shared_ptr<IPipelineFrame> mFrame;
  MBOOL mDirty;
  MINT32 mBatchMode;
  std::string mTraceName;
  std::map<StreamId_T, MWStream> mMWStreamMap;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_MWFRAME_H_
