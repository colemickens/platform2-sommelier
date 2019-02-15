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

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "V4L2TuningPipe"

#include "inc/V4L2TuningPipe.h"
#include "inc/V4L2PipeBase.h"
#include "mtkcam/utils/std/Log.h"

#include <cutils/compiler.h>  // for CC_LIKELY, CC_UNLIKELY
#include <map>
#include <memory>
#include <vector>

using NSCam::v4l2::V4L2StreamNode;

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

V4L2TuningPipe::V4L2TuningPipe(IspPipeType pipe_type,
                               MUINT32 sensor_idx,
                               const char* sz_caller_name)
    : V4L2PipeBase(pipe_type, sensor_idx, sz_caller_name),
      m_node_name("tuning") {}

MBOOL V4L2TuningPipe::init(const PipeTag pipe_tag) {
  std::lock_guard<std::mutex> lk_op(m_op_lock);
  MY_LOGD("+, pipe tag is %d", pipe_tag);

  const int cur_state = m_fsm_state.load(std::memory_order_relaxed);
  if (!checkFsm(cur_state, kOp_Init)) {
    MY_LOGI("wrong state to Init, current state is %d not Uninit", cur_state);
    return MFALSE;
  }

  MBOOL ret = V4L2PipeBase::init(pipe_tag);
  if (!ret) {
    MY_LOGE("call pipebase init fail");
    return MFALSE;
  }

  auto itr = mv_active_node.begin();
  while (itr != mv_active_node.end()) {
    /* only keep those node what we listend  */
    if (!v4l2::V4L2StreamNode::is_listened((*itr)->getId(), LISTENED_NODE_ID)) {
      /* if the node is not what we care, remove it from mv_active_node */
      MY_LOGD("found %s but not listened, erase it from active node",
              (*itr)->getName());
      itr = mv_active_node.erase(itr);
    } else {
      itr++;
    }
  }

  /* double check if there exists at least a video device */
  if (CC_UNLIKELY(mv_active_node.size() <= 0)) {
    MY_LOGE("no listened video devices");
    return MFALSE;
  }

  updateFsm(cur_state, kOp_Init);
  MY_LOGD("-, pipe tag is %d", pipe_tag);
  return MTRUE;
}

MBOOL V4L2TuningPipe::uninit() {
  std::lock_guard<std::mutex> lk_op(m_op_lock);
  MY_LOGD("+");

  const int cur_state = m_fsm_state.load(std::memory_order_relaxed);
  if (!checkFsm(cur_state, kOp_Uninit)) {
    MY_LOGI("wrong state to Uninit, current state is %d", cur_state);
    return MFALSE;
  }

  MBOOL ret = V4L2PipeBase::uninit();
  if (!ret) {
    MY_LOGE("call pipebase uninit fail");
    return MFALSE;
  }

  updateFsm(cur_state, kOp_Uninit);
  MY_LOGD("-");
  return MTRUE;
}

MBOOL V4L2TuningPipe::configPipe(
    const QInitParam& init_param,
    std::map<int, std::vector<std::shared_ptr<IImageBuffer> > >* map_vbuffers) {
  std::lock_guard<std::mutex> lk_op(m_op_lock);
  MY_LOGD("+");

  const int cur_state = m_fsm_state.load(std::memory_order_relaxed);
  if (!checkFsm(cur_state, kOp_Config)) {
    MY_LOGI("wrong state to config, current state is %d not init", cur_state);
    return MFALSE;
  }

  /* call base config */
  if (!V4L2PipeBase::configPipe(init_param, map_vbuffers))
    return MFALSE;

  updateFsm(cur_state, kOp_Config);
  MY_LOGD("-");
  return MTRUE;
}

}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam
