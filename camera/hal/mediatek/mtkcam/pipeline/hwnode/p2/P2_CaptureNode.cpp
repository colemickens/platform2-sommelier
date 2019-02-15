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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_CaptureNode.h"

#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG CaptureNode
#define P2_TRACE TRACE_P2_NODE

#include "P2_LogHeader.h"

#include <memory>

std::shared_ptr<P2CaptureNode> P2CaptureNode::createInstance(ePass2Type type) {
  if (type < 0 || type >= PASS2_TYPE_TOTAL) {
    MY_LOGE("not supported p2 type %d", type);
    return nullptr;
  }

  return std::make_shared<P2::P2CaptureNodeImp>(type);
}

P2::P2CaptureNodeImp::P2CaptureNodeImp(const ePass2Type pass2Type)
    : BaseNode() {
  MY_LOG_FUNC_ENTER("CaptureNode");
  mNodeName = "P2CaptureNode";  // default name
  MUINT32 logLevel = property_get_int32(KEY_P2_LOG, VAL_P2_LOG);
  mLog = NSCam::Utils::makeLogger("", "P2C", logLevel);
  mCaptureProcessor = std::make_shared<CaptureProcessor>();
  mP2Info = std::make_shared<P2InfoObj>(mLog);
  mP2Info->mConfigInfo.mP2Type = toP2Type(pass2Type);
  mP2Info->mConfigInfo.mLogLevel = logLevel;
  MY_LOG_FUNC_EXIT();
}

P2::P2CaptureNodeImp::~P2CaptureNodeImp() {
  MY_LOG_S_FUNC_ENTER(mLog);
  if (mStatus != STATUS_IDLE) {
    this->uninit();
  }
  MY_LOG_S_FUNC_EXIT(mLog);
}

MERROR P2::P2CaptureNodeImp::init(const InitParams& rParams) {
  ILog sensorLog = makeSensorLogger(mLog, rParams.openId);
  MY_LOG_S_FUNC_ENTER(sensorLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "CaptureNode::init");

  if (mStatus != STATUS_IDLE) {
    MY_S_LOGW(sensorLog, "cannot init: status[%d] != IDLE", mStatus);
    return INVALID_OPERATION;
  }

  MBOOL ret = MFALSE;

  mCaptureProcessor->setEnable(MTRUE);

  mP2Info->mConfigInfo.mMainSensorID = rParams.openId;
  mP2Info->mConfigInfo.mLog = sensorLog;
  mP2Info->mLog = sensorLog;

  mP2Info->addSensorInfo(sensorLog, rParams.openId);
  for (MUINT32 id : rParams.subOpenIdList) {
    if (id != rParams.openId) {
      ILog log = makeSensorLogger(mLog, id);
      mP2Info->addSensorInfo(log, id);
    }
  }

  mInIDMap = std::make_shared<P2InIDMap>(mP2Info->mConfigInfo.mAllSensorID,
                                         rParams.openId);

  ret = mCaptureProcessor->init(P2InitParam(
      P2Info(mP2Info, sensorLog, mP2Info->mConfigInfo.mMainSensorID)));

  if (ret) {
    mLog = sensorLog;
    mStatus = STATUS_READY;
    mOpenId = rParams.openId;
    mNodeId = rParams.nodeId;
    mNodeName = rParams.nodeName;

    MY_LOGD("OpenId %d, nodeId %#" PRIxPTR ", name %s", getOpenId(),
            getNodeId(), getNodeName());
  }

  MY_LOG_S_FUNC_EXIT(sensorLog);
  return ret ? OK : UNKNOWN_ERROR;
}

MERROR P2::P2CaptureNodeImp::uninit() {
  MY_LOG_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "CaptureNode::uninit");
  MBOOL ret = MFALSE;
  if (mStatus != STATUS_READY) {
    MY_S_LOGW(mLog, "cannot uninit: status[%d] != READY", mStatus);
  } else {
    mCaptureProcessor->uninit();
    mStatus = STATUS_IDLE;
    ret = MTRUE;
  }
  MY_LOG_S_FUNC_EXIT(mLog);
  return ret ? OK : UNKNOWN_ERROR;
}

MERROR P2::P2CaptureNodeImp::config(const ConfigParams& configParam) {
  MY_LOG_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "CaptureNode::config");
  MBOOL ret = MFALSE;
  if (mStatus != STATUS_READY) {
    MY_S_LOGW(mLog, "cannot config: status[%d] != READY", mStatus);
  } else {
    ret = parseConfigParam(configParam) &&
          mCaptureProcessor->config(P2ConfigParam(P2Info(
              mP2Info, mP2Info->mLog, mP2Info->mConfigInfo.mMainSensorID)));
  }

  MY_LOG_S_FUNC_EXIT(mLog);
  return ret ? OK : UNKNOWN_ERROR;
}

MERROR P2::P2CaptureNodeImp::queue(std::shared_ptr<IPipelineFrame> frame) {
  TRACE_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "CaptureNode::queue");
  MBOOL ret = MFALSE;

  MY_LOGD("req#:%d, frame#:%d", frame->getRequestNo(), frame->getFrameNo());

  if (mStatus != STATUS_READY) {
    MY_S_LOGW(mLog, "cannot queue: status[%d] != READY", mStatus);
  } else if (frame == nullptr) {
    MY_S_LOGW(mLog, "cannot queue: pipeline frame = NULL");
  } else {
    mRequestNo = frame->getRequestNo();
    MUINT32 uFrameNo = frame->getFrameNo();
    ILog frameLog =
        NSCam::Utils::makeFrameLogger(mLog, uFrameNo, mRequestNo, uFrameNo);
    std::shared_ptr<MWFrame> frameHolder =
        std::make_shared<MWFrame>(frameLog, mNodeId, mNodeName, frame);

    // MWFrame will handle callback even if enque failed
    std::shared_ptr<P2FrameRequest> pFrameRequest =
        createFrameRequest(frameLog, frameHolder);
    if (pFrameRequest != nullptr) {
      mCaptureProcessor->enque(pFrameRequest);
      ret = MTRUE;
    }
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret ? OK : UNKNOWN_ERROR;
}

MERROR P2::P2CaptureNodeImp::flush() {
  MY_LOG_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "CaptureNode::flush");
  MBOOL ret = MTRUE;
  if (mStatus != STATUS_READY) {
    MY_S_LOGW(mLog, "cannot flush: status[%d] != READY", mStatus);
    ret = MFALSE;
  } else {
    mCaptureProcessor->flush();
  }
  MY_LOG_S_FUNC_EXIT(mLog);
  return ret ? OK : UNKNOWN_ERROR;
}

MERROR P2::P2CaptureNodeImp::flush(
    const std::shared_ptr<IPipelineFrame>& frame) {
  MY_LOG_S_FUNC_ENTER(mLog);
  std::lock_guard<std::mutex> _lock(mMutex);
  P2_CAM_TRACE_NAME(TRACE_DEFAULT, "CaptureNode::flush");
  if (frame != nullptr) {
    if (frame->getRequestNo() == mRequestNo) {
      // Ensure that multi-frame request doesn't drop a frame
      queue(frame);
    } else {
      ILog log = NSCam::Utils::makeFrameLogger(mLog, frame->getFrameNo(),
                                               frame->getRequestNo(), 0);
      MWFrame::flushFrame(log, frame, mNodeId);
    }
  }
  MY_LOG_S_FUNC_EXIT(mLog);
  return OK;
}

P2Type P2::P2CaptureNodeImp::toP2Type(ePass2Type pass2type) const {
  P2Type type = P2_UNKNOWN;
  if (pass2type == PASS2_TIMESHARING) {
    type = P2_TIMESHARE_CAPTURE;
  } else {
    type = P2_CAPTURE;
  }
  return type;
}

MBOOL P2::P2CaptureNodeImp::parseConfigParam(const ConfigParams& configParam) {
  TRACE_S_FUNC_ENTER(mLog);
  MBOOL ret = MFALSE;
  std::shared_ptr<MWInfo> info = std::make_shared<MWInfo>(configParam);
  if (!info->isValid(mP2Info->mConfigInfo.mLog)) {
    MY_S_LOGW(mLog, "invalid config param");
  } else {
    mMWInfo = info;
    mP2Info = mP2Info->clone();
    mP2Info->mConfigInfo.mStreamConfigure = configParam.vStreamConfigure;
    updateConfigInfo(mP2Info, mMWInfo);
    ret = MTRUE;
  }
  TRACE_S_FUNC_EXIT(mLog);
  return ret;
}

MVOID P2::P2CaptureNodeImp::updateConfigInfo(
    const std::shared_ptr<P2InfoObj>& p2Info,
    const std::shared_ptr<MWInfo>& mwInfo) {
  TRACE_S_FUNC_ENTER(mLog);
  p2Info->mConfigInfo.mCustomOption = mwInfo->getCustomOption();
  TRACE_S_FUNC_EXIT(mLog);
}

std::shared_ptr<P2::P2FrameRequest> P2::P2CaptureNodeImp::createFrameRequest(
    const ILog& log, const std::shared_ptr<MWFrame>& frameHolder) {
  TRACE_S_FUNC_ENTER(log);
  std::shared_ptr<P2DataObj> p2Data = std::make_shared<P2DataObj>(log);
  p2Data->mFrameData.mP2FrameNo = log.getLogFrameID();
  P2Pack p2Pack(log, mP2Info, p2Data);
  std::shared_ptr<P2FrameRequest> request = std::make_shared<MWFrameRequest>(
      log, p2Pack, p2Data, mMWInfo, frameHolder, mInIDMap);
  TRACE_S_FUNC_EXIT(log);
  return request;
}
