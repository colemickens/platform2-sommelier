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
#define LOG_TAG "V4L2NormalPipe"

#undef THIS_NAME
#define THIS_NAME "V4L2NormalPipe"

#include <map>
#include <memory>
#include <vector>

#include "inc/V4L2NormalPipe.h"
#include "inc/V4L2PipeBase.h"
#include "mtkcam/utils/std/Log.h"

using NSCam::v4l2::V4L2StreamNode;

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

V4L2NormalPipe::V4L2NormalPipe(IspPipeType pipe_type,
                               MUINT32 sensor_idx,
                               const char* sz_caller_name)
    : V4L2PipeBase(pipe_type, sensor_idx, sz_caller_name) {}

MBOOL V4L2NormalPipe::init(const PipeTag pipe_tag) {
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

  /* occupy sensor  */
  PlatSensorsInfo& plat_sensor_info = mp_pipe_factory->m_plat_sensor_info;

  if (CC_UNLIKELY(plat_sensor_info.sensor_info[m_sensor_idx].occupied_owner)) {
    MY_LOGW("occupy sensor again, index: %d", m_sensor_idx);
  }

  plat_sensor_info.sensor_info[m_sensor_idx].occupied_owner =
      reinterpret_cast<MUINTPTR>(this);

  updateFsm(cur_state, kOp_Init);
  MY_LOGD("-, pipe tag is %d", pipe_tag);
  return MTRUE;
}

MBOOL V4L2NormalPipe::uninit() {
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

  if (!mp_pipe_factory) {
    MY_LOGE("pipe factory is NULL");
    return MFALSE;
  }

  /* release sensor occupation */
  mp_pipe_factory->m_plat_sensor_info.sensor_info[m_sensor_idx].occupied_owner =
      0;

  updateFsm(cur_state, kOp_Uninit);
  MY_LOGD("-");
  return MTRUE;
}

MBOOL V4L2NormalPipe::configPipe(
    const QInitParam& init_param,
    std::map<int, std::vector<std::shared_ptr<IImageBuffer> > >* map_vbuffers) {
  std::lock_guard<std::mutex> lk_op(m_op_lock);
  MY_LOGD("+");

  const int cur_state = m_fsm_state.load(std::memory_order_relaxed);
  if (!checkFsm(cur_state, kOp_Config)) {
    MY_LOGI("wrong state to config, current state is %d not init", cur_state);
    return MFALSE;
  }

  if (!mp_pipe_factory) {
    MY_LOGE("pipe factory is NULL");
    return MFALSE;
  }

  /* config sensor */
  if (!mup_halsensor) {
    IHalSensor* _halsensor = IHalSensorList::getInstance()->createSensor(
        THIS_NAME, 1, &m_sensor_idx);
    if (!_halsensor) {
      MY_LOGE("mpHalSensor Fail");
      return MFALSE;
    }

    auto deleter = [](IHalSensor* p_halsensor) {
      if (CC_LIKELY(p_halsensor != nullptr)) {
        p_halsensor->destroyInstance(THIS_NAME);
      }
    };
    mup_halsensor =
        std::unique_ptr<IHalSensor, decltype(deleter)>(_halsensor, deleter);
  } else {
    MY_LOGE("ERROR: ConfigPipe multiple times...");
    mup_halsensor.reset();
    return MFALSE;
  }

  m_sensor_config_params = init_param.mSensorCfg.at(0);

  if (!mup_halsensor->configure(1, &m_sensor_config_params)) {
    mup_halsensor.reset();
    MY_LOGE("fail when configure sensor driver");
    return MFALSE;
  }

  /* disable link if not configure LCSO */
  if (std::end(init_param.mPortInfo) ==
      std::find_if(std::begin(init_param.mPortInfo),
                   std::end(init_param.mPortInfo),
                   [](const NSCam::NSIoPipe::NSCamIOPipe::PortInfo& el) {
                     return el.mPortID.index == PORT_LCSO.index;
                   })) {
    if (disableLink(PORT_LCSO.index))
      MY_LOGI("Disable link of LCSO");
    else
      MY_LOGE("Fail to Disable link of LCSO");
  }

  /* disable link if not configure LMVO(EISO) */
  if (std::end(init_param.mPortInfo) ==
      std::find_if(std::begin(init_param.mPortInfo),
                   std::end(init_param.mPortInfo),
                   [](const NSCam::NSIoPipe::NSCamIOPipe::PortInfo& el) {
                     return el.mPortID.index == PORT_EISO.index;
                   })) {
    if (disableLink(PORT_EISO.index))
      MY_LOGI("Disable link of LMVO(EISO)");
    else
      MY_LOGE("Fail to Disable link of LMVO(EISO)");
  }

  /* call base config */
  if (!V4L2PipeBase::configPipe(init_param, map_vbuffers))
    return MFALSE;

  updateFsm(cur_state, kOp_Config);
  MY_LOGD("-");
  return MTRUE;
}

MBOOL V4L2NormalPipe::sendCommand(const int cmd,
                                  MINTPTR arg1,
                                  MINTPTR arg2,
                                  MINTPTR arg3) {
  switch (cmd) {
    case ENPipeCmd_GET_TG_OUT_SIZE: {
      /* prepare v4l2 control */
      MSize* sz_imgo = reinterpret_cast<MSize*>(arg1);
      if (!sz_imgo) {
        MY_LOGE("imgo size getter instance is null");
        return MFALSE;
      }

      if (!mup_halsensor) {
        MY_LOGE("sensor doesn't config yet");
        return MFALSE;
      }

      int _w = m_sensor_config_params.crop.w;
      int _h = m_sensor_config_params.crop.h;

      MY_LOGI("IMGO info(w,h)=(%d,%d)", _w, _h);

      /* index 0 is processed raw */
      sz_imgo[0].w = _w;
      sz_imgo[0].h = _h;
      /* index 1 is pure raw */
      sz_imgo[1].w = _w;
      sz_imgo[1].h = _h;
      break;
    }
    /* the caller must be P1 node */
    case ENPipeCmd_GEN_MAGIC_NUM: {
      uint32_t* rp_magicnum = reinterpret_cast<uint32_t*>(arg1);
      if (!msp_pipev4l2mgr) {
        MY_LOGE("v4l2 pipemgr is null");
        return MFALSE;
      }
      SyncReqMgr* pSyncReqMgr = msp_pipev4l2mgr->getSyncRegMgr();
      /* check arguments */
      if (CC_UNLIKELY(rp_magicnum == nullptr)) {
        MY_LOGE("arg1 is empty");
        return MFALSE;
      }
      if (CC_UNLIKELY(pSyncReqMgr == nullptr)) {
        MY_LOGE("cannot get SyncReqMgr");
        return MFALSE;
      }

      *rp_magicnum =
          pSyncReqMgr->acquireAvailableMagicNum(SyncReqMgr::SYNC_P1NODE);
      break;
    }
    default:
      MY_LOGW("not support this kind of cmd: %d", cmd);
      return MFALSE;
  }
  return MTRUE;
}

}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam
