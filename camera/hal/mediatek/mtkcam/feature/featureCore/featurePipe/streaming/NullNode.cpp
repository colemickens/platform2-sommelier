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
#define PIPE_CLASS_TAG "NullNode"
#define PIPE_TRACE TRACE_NULL_NODE
#include <featurePipe/common/include/PipeLog.h>
#include <featurePipe/streaming/NullNode.h>
#include <memory>

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {

NullNode::NullNode(const char* name) : StreamingFeatureNode(name) {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

NullNode::~NullNode() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

IOPolicyType NullNode::getIOPolicy(StreamType /*stream*/,
                                   const StreamingReqInfo& reqInfo) const {
  (void)reqInfo;
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return IOPOLICY_BYPASS;
}

MVOID NullNode::setInputBufferPool(const std::shared_ptr<IBufferPool>& pool,
                                   MUINT32 allocate) {
  (void)pool;
  (void)allocate;
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MVOID NullNode::setOutputBufferPool(const std::shared_ptr<IBufferPool>& pool,
                                    MUINT32 allocate) {
  (void)pool;
  (void)allocate;
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
}

MBOOL NullNode::onInit() {
  TRACE_FUNC_ENTER();
  StreamingFeatureNode::onInit();
  TRACE_FUNC_EXIT();
  return MFALSE;
}

MBOOL NullNode::onUninit() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MFALSE;
}

MBOOL NullNode::onThreadStart() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MFALSE;
}

MBOOL NullNode::onThreadStop() {
  TRACE_FUNC_ENTER();
  TRACE_FUNC_EXIT();
  return MFALSE;
}

MBOOL NullNode::onThreadLoop() {
  TRACE_FUNC("Waitloop");
  if (!waitAllQueue()) {
    return MFALSE;
  }
  TRACE_FUNC_EXIT();
  return MFALSE;
}

}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam
