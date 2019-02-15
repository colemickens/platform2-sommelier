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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_STREAMINGNODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_STREAMINGNODE_H_

#include <mtkcam/pipeline/hwnode/P2StreamingNode.h>

#include "P2_MWFrame.h"
#include "P2_MWFrameRequest.h"
#include "P2_DispatchProcessor.h"
#include "BaseNode.h"

#include <memory>
#include <string>

using NSCam::v3::P2StreamingNode;
using NSCam::v3::P2Common::UsageHint;

#define INVALID_OPEN_ID (-1)

namespace P2 {

class P2StreamingNodeImp : virtual public NSCam::v3::BaseNode,
                           virtual public P2StreamingNode {
 public:
  P2StreamingNodeImp(const P2StreamingNode::ePass2Type pass2Type,
                     const UsageHint& usageHint);
  virtual ~P2StreamingNodeImp();
  virtual NSCam::MERROR init(const IPipelineNode::InitParams& initParam);
  virtual NSCam::MERROR uninit();
  virtual NSCam::MERROR config(
      const P2StreamingNode::ConfigParams& configParam);
  virtual NSCam::MERROR queue(std::shared_ptr<IPipelineFrame> frame);
  virtual NSCam::MERROR kick();
  virtual NSCam::MERROR flush();
  virtual NSCam::MERROR flush(const std::shared_ptr<IPipelineFrame>& frame);

  virtual MINT32 getOpenId() const;
  virtual NodeId_T getNodeId() const;
  virtual char const* getNodeName() const;

 private:
  P2Type toP2Type(P2StreamingNode::ePass2Type pass2type,
                  const UsageHint& hint) const;
  P2UsageHint toP2UsageHint(const UsageHint& hint) const;

  MUINT32 generateFrameID();
  MBOOL parseInitParam(const ILog& log,
                       const IPipelineNode::InitParams& initParam);
  MBOOL parseConfigParam(const P2StreamingNode::ConfigParams& configParam);
  MVOID updateConfigInfo(const std::shared_ptr<P2InfoObj>& p2Info,
                         const std::shared_ptr<MWInfo>& mwInfo);
  MBOOL prepareFrameRequest(const ILog& log,
                            std::shared_ptr<P2FrameRequest>* param,
                            const std::shared_ptr<MWFrame>& frameHolder);
  MVOID printIOMap(const IPipelineFrame::InfoIOMapSet& ioMap);

 private:
  std::mutex mMutex;
  enum P2StreamingNodeStatus { STATUS_IDLE, STATUS_READY };
  P2StreamingNodeStatus mStatus = STATUS_IDLE;
  IPipelineNode::NodeId_T mNodeID = NULL;
  NodeName_T mNodeName = std::string("P2_StreamingNode");

  ILog mLog;
  std::shared_ptr<P2InfoObj> mP2Info;
  std::shared_ptr<MWInfo> mMWInfo;
  std::shared_ptr<P2InIDMap> mInIDMap;
  std::shared_ptr<DispatchProcessor> mDispatcher;
  MUINT32 mFrameCount = 0;
  P2UsageHint mP2UsageHint;
};

}  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_STREAMINGNODE_H_
