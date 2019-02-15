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

#include <featurePipe/common/include/DebugControl.h>
#define PIPE_CLASS_TAG "HelperNode"
#define PIPE_TRACE TRACE_HELPER_NODE
#include <featurePipe/common/include/PipeLog.h>
#include <mtkcam/feature/featureCore/featurePipe/streaming/HelperNode.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

HelperNode::HelperNode(const char* name) : StreamingFeatureNode(name) {
  TRACE_FUNC_ENTER();
  this->addWaitQueue(&mCBRequests);
  TRACE_FUNC_EXIT();
}

HelperNode::~HelperNode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL HelperNode::onData(DataID id, const HelperData& data) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC("Frame %d: %s arrived", data.mRequest->mRequestNo, ID2Name(id));
  MBOOL ret = MFALSE;
  if (id == ID_P2A_TO_HELPER || id == ID_PMDP_TO_HELPER ||
      id == ID_BOKEH_TO_HELPER || id == ID_WARP_TO_HELPER ||
      id == ID_VMDP_TO_HELPER || id == ID_RSC_TO_HELPER ||
      id == ID_FOV_WARP_TO_HELPER || id == ID_N3D_TO_HELPER ||
      id == ID_DUMMY_TO_NEXT_FULLIMG) {
    mCBRequests.enque(data);
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL HelperNode::onInit() {
  TRACE_FUNC_ENTER();
  StreamingFeatureNode::onInit();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL HelperNode::onUninit() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL HelperNode::onThreadStart() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL HelperNode::onThreadStop() {
  TRACE_FUNC_ENTER();
  clearTSQ();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL HelperNode::onThreadLoop() {
  P2_CAM_TRACE_CALL(TRACE_DEFAULT);
  TRACE_FUNC("Waitloop");
  {
    HelperData cbRequest;
    RequestPtr request;
    if (!waitAllQueue()) {
      return MFALSE;
    }
    if (!mCBRequests.deque(&cbRequest)) {
      MY_LOGE("Request deque out of sync");
      return MFALSE;
    }
    if (cbRequest.mRequest == NULL) {
      MY_LOGE("Request out of sync");
      return MFALSE;
    }
    TRACE_FUNC_ENTER();
    request = cbRequest.mRequest;
    request->mTimer.resumeHelper();
    TRACE_FUNC("Frame %d in Helper", request->mRequestNo);
    P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "processHelper");
    processHelper(request, cbRequest.mData);
    P2_CAM_TRACE_END(TRACE_ADVANCED);
    request->mTimer.stopHelper();
    P2_CAM_TRACE_BEGIN(TRACE_ADVANCED, "release datas");
  }
  P2_CAM_TRACE_END(TRACE_ADVANCED);
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL needDelayCallback(FeaturePipeParam::MSG_TYPE msg) {
  return (msg == FeaturePipeParam::MSG_DISPLAY_DONE) ||
         (msg == FeaturePipeParam::MSG_FRAME_DONE);
}

MVOID HelperNode::storeMessage(const RequestPtr& request,
                               FeaturePipeParam::MSG_TYPE msg) {
  switch (msg) {
    case FeaturePipeParam::MSG_DISPLAY_DONE:
      request->mHelperNodeData.markMsgReceived(HelperRWData::Msg_Display_Done);
      break;
    case FeaturePipeParam::MSG_FRAME_DONE:
      request->mHelperNodeData.markMsgReceived(HelperRWData::Msg_Frame_Done);
      break;
    default:
      MY_LOGE("store unknown msg(%d), do nothing.", msg);
      break;
  }
}

MVOID HelperNode::handleStoredMessage(const RequestPtr& request) {
  if (request->mHelperNodeData.isMsgReceived(HelperRWData::Msg_Display_Done)) {
    processCB(request, FeaturePipeParam::MSG_DISPLAY_DONE);
  }
  if (request->mHelperNodeData.isMsgReceived(HelperRWData::Msg_Frame_Done)) {
    processCB(request, FeaturePipeParam::MSG_FRAME_DONE);
  }
}

MBOOL HelperNode::processHelper(const RequestPtr& request,
                                const HelpReq& helpReq) {
  TRACE_FUNC_ENTER();
  if (helpReq.mInternalMsg == HelpReq::MSG_UNKNOWN) {
    processCB(request, helpReq.mCBMsg);
  } else if (helpReq.mInternalMsg == HelpReq::MSG_PMDP_DONE) {
    request->mHelperNodeData.markMsgReceived(HelperRWData::Msg_PMDP_Done);
    handleStoredMessage(request);
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL HelperNode::processCB(const RequestPtr& request,
                            FeaturePipeParam::MSG_TYPE msg) {
  TRACE_FUNC_ENTER();
  prepareCB(request, msg);

  if (msg == FeaturePipeParam::MSG_INVALID) {
    MY_LOGE("Receive Invalid Msg callback!! req(%d)", request->mRequestNo);
  } else {
    request->doExtCallback(msg);
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MVOID HelperNode::prepareCB(const RequestPtr& request,
                            FeaturePipeParam::MSG_TYPE msg) {
  switch (msg) {
    case FeaturePipeParam::MSG_DISPLAY_DONE:
      request->mTimer.markDisplayDone();
      break;
    case FeaturePipeParam::MSG_FRAME_DONE:
      request->mTimer.markDisplayDone();
      request->mTimer.markFrameDone();
      break;
    case FeaturePipeParam::MSG_RSSO_DONE:
      break;
    default:
      break;
  }
}

MVOID HelperNode::clearTSQ() {
  TRACE_FUNC_ENTER();
  mTSQueue.clear();
  TRACE_FUNC_EXIT();
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
