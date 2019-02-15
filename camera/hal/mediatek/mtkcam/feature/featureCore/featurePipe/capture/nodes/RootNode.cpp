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

#include <common/include/DebugControl.h>

#define PIPE_CLASS_TAG "RootNode"
#define PIPE_TRACE TRACE_ROOT_NODE
#include <capture/nodes/RootNode.h>
#include <common/include/PipeLog.h>

#include <custom/debug_exif/dbg_exif_param.h>
#include <mtkcam/utils/metadata/hal/mtk_platform_metadata_tag.h>
#include "base/strings/stringprintf.h"
#include <mtkcam/drv/IHalSensor.h>
#include <vector>

#define CHECK_OBJECT(x)              \
  do {                               \
    if (x == nullptr) {              \
      MY_LOGE("Null %s Object", #x); \
      return -MFALSE;                \
    }                                \
  } while (0)

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {
namespace NSCapture {

RootNode::RootNode(NodeID_T nid, const char* name)
    : CaptureFeatureNode(nid, name) {
  TRACE_FUNC_ENTER();
  this->addWaitQueue(&mRequests);
  TRACE_FUNC_EXIT();
}

RootNode::~RootNode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL RootNode::onData(DataID id, const RequestPtr& pRequest) {
  TRACE_FUNC_ENTER();
  MY_LOGD_IF(1, "Frame %d: %s arrived", pRequest->getRequestNo(),
             PathID2Name(id));

  MBOOL ret = MFALSE;
  std::lock_guard<std::mutex> _l(mLock);
  switch (id) {
    case PID_ENQUE: {
      dispatch(pRequest);
    }

      ret = MTRUE;
      break;
    default:
      ret = MFALSE;
      MY_LOGE("unknown data id :%d", id);
      break;
  }

  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL RootNode::onInit() {
  TRACE_FUNC_ENTER();
  CaptureFeatureNode::onInit();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL RootNode::onThreadStart() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL RootNode::onThreadStop() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MERROR RootNode::evaluate(CaptureFeatureInferenceData* rInference) {
  return OK;
}

MBOOL RootNode::onThreadLoop() {
  TRACE_FUNC_ENTER();

  // block until queue ready, or flush() breaks the blocking state too.
  if (!waitAllQueue()) {
    return MFALSE;
  }

  {
    std::lock_guard<std::mutex> _l(mLock);

    RequestPtr pRequest = nullptr;
    if (!mRequests.deque(&pRequest)) {
      MY_LOGD("mRequests.deque() failed");
      return MFALSE;
    }

    this->incExtThreadDependency();

    mvPendingRequests.push_back(pRequest);

    auto frameNum = pRequest->getParameter(PID_FRAME_COUNT);
    if ((ssize_t)mvPendingRequests.size() < frameNum) {
      MY_LOGD("(%d < %d) data not ready, keep waiting",
              mvPendingRequests.size(), frameNum);
    } else {
      MY_LOGD("The requests are ready");
      std::vector<RequestPtr> vReadyRequests = mvPendingRequests;
      mvPendingRequests.clear();
      MY_LOGE("Not support BSS!");
    }
  }

  this->decExtThreadDependency();

  TRACE_FUNC_EXIT();
  return MTRUE;
}

/*******************************************************************************
 *
 ****************************************************************************/
MVOID RootNode::reorder(const std::vector<RequestPtr>& rvRequests,
                        const std::vector<RequestPtr>& rvOrderedRequests) {
  if (rvRequests.size() != rvOrderedRequests.size()) {
    MY_LOGE("input(%zu) != result(%zu)", rvRequests.size(),
            rvOrderedRequests.size());
    return;
  }

  // Switch input buffers with each other. To keep the first request's data path
  rvRequests[0]->setCrossRequest(rvOrderedRequests[0]);
  rvOrderedRequests[0]->setCrossRequest(rvRequests[0]);

  for (size_t i = 0; i < rvOrderedRequests.size(); i++) {
    if (i == 0) {
      dispatch(rvRequests[0]);
    } else if (rvOrderedRequests[i] == rvRequests[0]) {
      dispatch(rvOrderedRequests[0]);
    } else {
      dispatch(rvOrderedRequests[0]);
    }
  }
}

}  // namespace NSCapture
}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
