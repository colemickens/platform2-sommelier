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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_StreamingNode.h"

#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG StreamingNode
#define P2_TRACE TRACE_P2_NODE
#include "P2_LogHeader.h"

#include <memory>

std::shared_ptr<P2StreamingNode> P2StreamingNode::createInstance(
    P2StreamingNode::ePass2Type const type,
    NSCam::v3::P2Common::UsageHint usage) {
  if (type < 0 || type >= PASS2_TYPE_TOTAL) {
    MY_LOGE("not supported p2 type %d", type);
    return nullptr;
  }

  return std::make_shared<P2::P2StreamingNodeImp>(type, usage);
}

namespace P2 {

P2StreamingNodeImp::P2StreamingNodeImp(
    const P2StreamingNode::ePass2Type pass2Type,
    const NSCam::v3::P2Common::UsageHint& usageHint)
    : BaseNode(), P2StreamingNode() {
  MY_LOG_FUNC_ENTER("StreamingNode");
  MUINT32 logLevel = property_get_int32(KEY_P2_LOG, VAL_P2_LOG);
  mLog = NSCam::Utils::makeLogger("", "P2S", logLevel);
  mDispatcher = std::make_shared<DispatchProcessor>();
  mP2Info = std::make_shared<P2InfoObj>(mLog);
  mP2Info->mConfigInfo.mP2Type = toP2Type(pass2Type, usageHint);
  mP2Info->mConfigInfo.mUsageHint = toP2UsageHint(usageHint);
  mP2Info->mConfigInfo.mLogLevel = logLevel;
  MY_LOG_FUNC_EXIT();
}

P2StreamingNodeImp::~P2StreamingNodeImp() {
  MY_LOG_S_FUNC_ENTER(mLog);
  if (mStatus != STATUS_IDLE) {
    this->uninit();
  }
  MY_LOG_S_FUNC_EXIT(mLog);
}

NSCam::MERROR P2StreamingNodeImp::init(
    const IPipelineNode::InitParams& initParam) {
  ILog sensorLog = NSCam::Utils::makeSensorLogger(mLog, initParam.openId);
  MY_LOG_S_FUNC_ENTER(sensorLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "StreamingNode::init");
  MBOOL ret = MFALSE;
  if (mStatus != STATUS_IDLE) {
    MY_S_LOGW(sensorLog, "cannot init: status[%d] != IDLE", mStatus);
  } else {
    mDispatcher->setNeedThread(MFALSE);
    ret = parseInitParam(sensorLog, initParam) &&
          mDispatcher->init(P2InitParam(
              P2Info(mP2Info, sensorLog, mP2Info->mConfigInfo.mMainSensorID)));
    if (ret) {
      mLog = sensorLog;
      mStatus = STATUS_READY;
    }
  }
  MY_LOG_S_FUNC_EXIT(sensorLog);
  return ret ? OK : UNKNOWN_ERROR;
}

NSCam::MERROR P2StreamingNodeImp::uninit() {
  MY_LOG_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "StreamingNode::uninit");
  MBOOL ret = MFALSE;
  if (mStatus != STATUS_READY) {
    MY_S_LOGW(mLog, "cannot uninit: status[%d] != READY", mStatus);
  } else {
    mDispatcher->uninit();
    mStatus = STATUS_IDLE;
    ret = MTRUE;
  }
  MY_LOG_S_FUNC_EXIT(mLog);
  return ret ? OK : UNKNOWN_ERROR;
}

NSCam::MERROR P2StreamingNodeImp::config(
    const P2StreamingNode::ConfigParams& configParam) {
  MY_LOG_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "StreamingNode::config");
  MBOOL ret = MFALSE;
  if (mStatus != STATUS_READY) {
    MY_S_LOGW(mLog, "cannot config: status[%d] != READY", mStatus);
  } else {
    ret = parseConfigParam(configParam) &&
          mDispatcher->config(P2ConfigParam(P2Info(
              mP2Info, mP2Info->mLog, mP2Info->mConfigInfo.mMainSensorID)));
  }
  MY_LOG_S_FUNC_EXIT(mLog);
  return ret ? OK : UNKNOWN_ERROR;
}

NSCam::MERROR P2StreamingNodeImp::queue(std::shared_ptr<IPipelineFrame> frame) {
  TRACE_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  MBOOL ret = MFALSE;
  if (mStatus != STATUS_READY) {
    MY_LOGE("cannot queue: status[%d] != READY", mStatus);
  } else if (frame == nullptr) {
    MY_LOGE("cannot queue: pipeline frame = NULL");
  } else {
    std::shared_ptr<MWFrame> frameHolder;

    P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "StreamingNode:queue->newMWFrame");
    MUINT32 frameID = generateFrameID();
    ILog frameLog = NSCam::Utils::makeFrameLogger(
        mLog, frame->getFrameNo(), frame->getRequestNo(), frameID);
    frameHolder =
        std::make_shared<MWFrame>(frameLog, mNodeID, mNodeName, frame);
    P2_CAM_TRACE_END(TRACE_ADVANCED);
    if (frameHolder == nullptr) {
      MY_LOGE("@@ OOM: allocate MWFrame failed");
    } else {
      // MWFrame will handle callback even if enque failed
      ret = MTRUE;
      std::shared_ptr<P2FrameRequest> request;
      if (prepareFrameRequest(frameLog, &request, frameHolder)) {
        mDispatcher->enque(request);
      }
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret ? OK : UNKNOWN_ERROR;
}

NSCam::MERROR P2StreamingNodeImp::kick() {
  return OK;
}

NSCam::MERROR P2StreamingNodeImp::flush() {
  MY_LOG_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "StreamingNode::flush");
  MBOOL ret = MTRUE;
  if (mStatus != STATUS_READY) {
    MY_S_LOGW(mLog, "cannot flush: status[%d] != READY", mStatus);
    ret = MFALSE;
  } else {
    mDispatcher->flush();
  }
  MY_LOG_S_FUNC_EXIT(mLog);
  return ret ? OK : UNKNOWN_ERROR;
}

NSCam::MERROR P2StreamingNodeImp::flush(
    const std::shared_ptr<IPipelineFrame>& frame) {
  MY_LOG_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "StreamingNode::flush");
  if (frame != nullptr) {
    ILog log = NSCam::Utils::makeFrameLogger(mLog, frame->getFrameNo(),
                                             frame->getRequestNo(), 0);
    MWFrame::flushFrame(log, frame, mNodeID);
  }
  MY_LOG_S_FUNC_EXIT(mLog);
  return OK;
}

MINT32 P2StreamingNodeImp::getOpenId() const {
  TRACE_S_FUNC_ENTER(mLog);
  TRACE_S_FUNC_EXIT(mLog);
  return mP2Info->mConfigInfo.mMainSensorID;
}

P2StreamingNodeImp::NodeId_T P2StreamingNodeImp::getNodeId() const {
  TRACE_S_FUNC_ENTER(mLog);
  TRACE_S_FUNC_EXIT(mLog);
  return mNodeID;
}

char const* P2StreamingNodeImp::getNodeName() const {
  TRACE_S_FUNC_ENTER(mLog);
  TRACE_S_FUNC_EXIT(mLog);
  return mNodeName.c_str();
}

P2Type P2StreamingNodeImp::toP2Type(
    P2StreamingNode::ePass2Type /*pass2type*/,
    const NSCam::v3::P2Common::UsageHint& hint) const {
  TRACE_S_FUNC_ENTER(mLog);
  P2Type type = P2_UNKNOWN;

  if (hint.mAppMode == NSCam::v3::P2Common::APP_MODE_VIDEO) {
    type = P2_VIDEO;
  } else if (hint.mAppMode == NSCam::v3::P2Common::APP_MODE_HIGH_SPEED_VIDEO) {
    type = P2_HS_VIDEO;
  } else {
    type = P2_PHOTO;
  }
  TRACE_S_FUNC_EXIT(mLog);
  return type;
}

P2UsageHint P2StreamingNodeImp::toP2UsageHint(
    const NSCam::v3::P2Common::UsageHint& hint) const {
  TRACE_S_FUNC_ENTER(mLog);
  P2UsageHint usage;

  usage.mStreamingSize = hint.mStreamingSize;
  usage.m3DNRMode = hint.m3DNRMode;
  usage.mUseTSQ = hint.mUseTSQ;
  usage.mDynamicTuning = property_get_int32("vendor.debug.p2.dynamicTuning", 1);

  usage.mOutCfg.mMaxOutNum = hint.mOutCfg.mMaxOutNum;
  usage.mOutCfg.mHasPhysical = hint.mOutCfg.mHasPhysical;
  usage.mOutCfg.mHasLarge = hint.mOutCfg.mHasLarge;
  TRACE_S_FUNC_EXIT(mLog);
  return usage;
}

MUINT32 P2StreamingNodeImp::generateFrameID() {
  TRACE_S_FUNC_ENTER(mLog);
  TRACE_S_FUNC_EXIT(mLog);
  return ++mFrameCount;
}

MBOOL P2StreamingNodeImp::parseInitParam(
    const ILog& log, const IPipelineNode::InitParams& initParam) {
  TRACE_S_FUNC_ENTER(log);
  MBOOL ret = MTRUE;
  this->mNodeID = initParam.nodeId;
  this->mNodeName = initParam.nodeName;
  mP2Info->mConfigInfo.mMainSensorID = initParam.openId;
  mP2Info->mConfigInfo.mLog = log;
  mP2Info->mLog = log;

  mP2Info->addSensorInfo(log, initParam.openId);
  for (MUINT32 id : initParam.subOpenIdList) {
    if (id != (MUINT32)initParam.openId) {
      ILog sensorLog = makeSensorLogger(log, id);
      mP2Info->addSensorInfo(sensorLog, id);
    }
  }

  mInIDMap = std::make_shared<P2InIDMap>(mP2Info->mConfigInfo.mAllSensorID,
                                         initParam.openId);
  TRACE_S_FUNC_EXIT(log);
  return ret;
}

MBOOL P2StreamingNodeImp::parseConfigParam(
    const P2StreamingNode::ConfigParams& configParam) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  std::shared_ptr<MWInfo> info = std::make_shared<MWInfo>(configParam);
  if (info == nullptr) {
    MY_LOGE("OOM: allocate MWInfo failed");
  } else if (!info->isValid(mP2Info->mConfigInfo.mLog)) {
    MY_LOGE("invalid config param");
  } else {
    mMWInfo = info;
    mP2Info = mP2Info->clone();
    mP2Info->mConfigInfo.mUsageHint = toP2UsageHint(configParam.mUsageHint);
    mP2Info->mConfigInfo.mStreamConfigure = configParam.vStreamConfigure;

    updateConfigInfo(mP2Info, mMWInfo);
    ret = MTRUE;
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MVOID P2StreamingNodeImp::updateConfigInfo(
    const std::shared_ptr<P2InfoObj>& p2Info,
    const std::shared_ptr<MWInfo>& mwInfo) {
  TRACE_S_FUNC_ENTER(mLog);
  p2Info->mConfigInfo.mBurstNum = mwInfo->getBurstNum();
  p2Info->mConfigInfo.mCustomOption = mwInfo->getCustomOption();
  TRACE_S_FUNC_EXIT(mLog);
}

MBOOL P2StreamingNodeImp::prepareFrameRequest(
    const ILog& log,
    std::shared_ptr<P2FrameRequest>* request,
    const std::shared_ptr<MWFrame>& frameHolder) {
  TRACE_S_FUNC_ENTER(log);
  MBOOL ret = MTRUE;
  std::shared_ptr<P2DataObj> p2Data = std::make_shared<P2DataObj>(log);
  p2Data->mFrameData.mP2FrameNo = log.getLogFrameID();
  P2Pack p2Pack(log, mP2Info, p2Data);
  P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "new MWFrameRequest");
  *request = std::make_shared<MWFrameRequest>(log, p2Pack, p2Data, mMWInfo,
                                              frameHolder, mInIDMap);
  P2_CAM_TRACE_END(TRACE_ADVANCED);
  if (*request == nullptr) {
    MY_LOGE("prepareFrameRequest OOM: allocate MWFrameRequest failed");
    ret = MFALSE;
  }
  TRACE_S_FUNC_EXIT(log);
  return ret;
}

}  // namespace P2
