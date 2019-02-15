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
#define LOG_TAG "V4L2StatisticPipe"

#include "inc/V4L2StatisticPipe.h"
#include "inc/V4L2PipeBase.h"
#include "mtkcam/utils/std/Log.h"

#include <map>
#include <memory>
#include <vector>

using NSCam::v4l2::V4L2StreamNode;

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

V4L2StatisticPipe::V4L2StatisticPipe(IspPipeType pipe_type,
                                     MUINT32 sensor_idx,
                                     const char* sz_caller_name)
    : V4L2PipeBase(pipe_type, sensor_idx, sz_caller_name),
      m_pipe_type(pipe_type) {}

MBOOL V4L2StatisticPipe::init(const PipeTag pipe_tag) {
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
    if (!v4l2::V4L2StreamNode::is_listened((*itr)->getId(),
                                           LISTENED_NODE_ID(m_pipe_type))) {
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

MBOOL V4L2StatisticPipe::uninit() {
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

MBOOL V4L2StatisticPipe::configPipe(
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

MBOOL V4L2StatisticPipe::sendCommand(int cmd,
                                     MINTPTR arg1,
                                     MINTPTR arg2,
                                     MINTPTR arg3) {
  switch (cmd) {
    case ENPipeCmd_SET_META2_DISABLED:
      if (disableLink(PORT_META2.index)) {
        MY_LOGD("disable link of META2 succeeded");
        return MTRUE;
      } else {
        MY_LOGW("disable link of META2 failed");
        return MFALSE;
      }
    default:
      return V4L2PipeBase::sendCommand(cmd, arg1, arg2, arg3);
  }
  return MFALSE;
}

}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam
