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
#define PIPE_CLASS_TAG "RootNode"
#define PIPE_TRACE TRACE_ROOT_NODE
#include <featurePipe/common/include/PipeLog.h>
#include <featurePipe/streaming/RootNode.h>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

RootNode::RootNode(const char* name) : StreamingFeatureNode(name) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

RootNode::~RootNode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL RootNode::onData(DataID id, const RequestPtr& data) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC("Frame %d: %s arrived", data->mRequestNo, ID2Name(id));
  MBOOL ret = MFALSE;
  if (id == ID_ROOT_ENQUE) {
    DataID firstP2ID = ID_ROOT_TO_P2A;
    handleData(firstP2ID, data);
    ret = MTRUE;
  }
  TRACE_FUNC_EXIT();
  return ret;
}

MBOOL RootNode::onInit() {
  TRACE_FUNC_ENTER();
  StreamingFeatureNode::onInit();
  TRACE_FUNC_EXIT();
  return MTRUE;
}

MBOOL RootNode::onThreadLoop() {
  TRACE_FUNC_ENTER();
  if (!waitAllQueue()) {
    TRACE_FUNC("Wait all queue exit");
    return MFALSE;
  }
  TRACE_FUNC_EXIT();
  return MTRUE;
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
