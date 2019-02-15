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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_CAPTURENODE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_CAPTURENODE_H_

#include <mtkcam/pipeline/hwnode/P2CaptureNode.h>

#include "P2_Common.h"
#include "P2_MWFrame.h"
#include "P2_MWFrameRequest.h"
#include "P2_CaptureProcessor.h"
#include "BaseNode.h"

#include <memory>

using NSCam::v3::P2CaptureNode;

/******************************************************************************
 *
 ******************************************************************************/
namespace P2 {

/******************************************************************************
 *
 ******************************************************************************/
class P2CaptureNodeImp : virtual public NSCam::v3::BaseNode,
                         public virtual P2CaptureNode {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  explicit P2CaptureNodeImp(ePass2Type const type);

  ~P2CaptureNodeImp();

  virtual NSCam::MERROR config(ConfigParams const& rParams);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineNode Interface.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:
  virtual NSCam::MERROR init(InitParams const& rParams);

  virtual NSCam::MERROR uninit();

  virtual NSCam::MERROR flush();

  virtual NSCam::MERROR flush(std::shared_ptr<IPipelineFrame> const& pFrame);

  virtual NSCam::MERROR queue(std::shared_ptr<IPipelineFrame> pFrame);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:
  NSCam::MERROR verifyConfigParams(ConfigParams const& rParams) const;

 private:
  P2Type toP2Type(ePass2Type pass2type) const;

  MBOOL parseInitParam(const ILog& log,
                       const IPipelineNode::InitParams& rParam);
  MBOOL parseConfigParam(const ConfigParams& rParam);
  MVOID updateConfigInfo(const std::shared_ptr<P2InfoObj>& p2Info,
                         const std::shared_ptr<MWInfo>& mwInfo);
  std::shared_ptr<P2FrameRequest> createFrameRequest(
      const ILog& log, const std::shared_ptr<MWFrame>& frameHolder);
  MVOID printIOMap(const IPipelineFrame::InfoIOMapSet& ioMap);

 private:
  std::mutex mMutex;
  enum P2CaptureNodeStatus { STATUS_IDLE, STATUS_READY };
  P2CaptureNodeStatus mStatus = STATUS_IDLE;

  ILog mLog;
  std::shared_ptr<P2InfoObj> mP2Info;
  std::shared_ptr<MWInfo> mMWInfo;
  std::shared_ptr<P2InIDMap> mInIDMap;
  MUINT32 mRequestNo;
  std::shared_ptr<CaptureProcessor> mCaptureProcessor;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace P2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_CAPTURENODE_H_
