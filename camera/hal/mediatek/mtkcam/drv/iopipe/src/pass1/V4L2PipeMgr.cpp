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
#define LOG_TAG "V4L2PipeMgr"

#include <cutils/compiler.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "inc/V4L2PipeMgr.h"
#include "mtkcam/utils/std/Log.h"
#include "V4L2StreamNode.h"

using NSCam::NSIoPipe::NSCamIOPipe::V4L2PipeMgr;
using NSCam::v4l2::SyncReqMgr;

V4L2PipeMgr::V4L2PipeMgr(MUINT32 sensor_idx)
    : m_device_tag(MEDIA_CONTROLLER_P1_Unused),
      m_media_device_index(-1),
      m_pipe_tag(kPipeTag_Num),
      m_control(nullptr),
      m_sensor_idx(sensor_idx) {}

MBOOL V4L2PipeMgr::buildV4l2Links(PipeTag pipe_tag) {
  const ScenarioInfo* scen_info =
      std::find_if(std::begin(m_scenario_mapper), std::end(m_scenario_mapper),
                   [pipe_tag](const ScenarioInfo& _scen_info) {
                     return _scen_info.pipe_tag == pipe_tag;
                   });
  if (scen_info == std::end(m_scenario_mapper)) {
    MY_LOGE("wrong input, pipe tag is %d", pipe_tag);
    return MFALSE;
  }

  MY_LOGI("Device Tag: %s, Stream Tag: %s", scen_info->device_name.c_str(),
          scen_info->pipe_name.c_str());

  /* create MtkCameraV4L2API to build links */
  MtkCameraV4L2API* pControl = new MtkCameraV4L2API;
  int media_device;
  const bool enable_tuning = is_enable_tuning(pipe_tag);
  MY_LOGI("enable tuning = %d", enable_tuning);
  media_device = pControl->openAndsetupAllLinks(
      scen_info->device_tag, &mv_media_entity, enable_tuning);
  if (media_device < 0) {
    delete pControl;  // release resource
    MY_LOGE("Fail to openAndsetupAllLinks (ret = %d)", media_device);
    return MFALSE;
  }

  /* ok, saves MtkCameraV4L2API */
  do {
    /* declare a customized deleter (RAII idiom) */
    auto deleter = [media_device](MtkCameraV4L2API* pControl) {
      if (CC_LIKELY(pControl != nullptr)) {
        pControl->resetAllLinks(media_device);
        pControl->closeMediaDevice(media_device);
        delete pControl;
      }
    };

    std::shared_ptr<MtkCameraV4L2API> _control(pControl, std::move(deleter));

    m_control = std::move(_control);
    if (!m_control) {
      MY_LOGE("unsupported pipe tag %d", pipe_tag);
      return MFALSE;
    }
  } while (0);

  auto isKeepDevice = [](int type) -> bool {
    switch (type) {
      case DEVICE_VIDEO:
      case SUBDEV_GENERIC:
        return true;
      default:
        break;
    }
    return false;
  };

  uint32_t usersid_of_syncreqmgr = SyncReqMgr::SYNC_NONE;

  for (const auto& entity : mv_media_entity) {
    std::shared_ptr<V4L2Device> device;
    const std::string name(entity->getName());
    MY_LOGI("device name: %s", name.c_str());

    if (!isKeepDevice(entity->getType())) {
      continue;
    }
    if (entity->getDevice(&device) != NO_ERROR) {
      MY_LOGE("get device error");
      return MFALSE;
    }

    /* pick up p1 subdev */
    if (entity->getType() == SUBDEV_GENERIC) {
      m_p1_subdev = std::static_pointer_cast<V4L2VideoNode>(device);
      continue;  // do not add subdev to mv_active_node
    }

    std::shared_ptr<NSCam::v4l2::V4L2StreamNode> node =
        std::make_shared<NSCam::v4l2::V4L2StreamNode>(
            std::static_pointer_cast<V4L2VideoNode>(device), name);
    mv_active_node.push_back(node);

    /* constructs users id for SyncReqMgr */
    usersid_of_syncreqmgr |= SyncReqMgr::getSyncIdByNodeId(node->getId());
  }

  /* create Sync Request Manager */
  m_syncReqMgr.reset(
      new SyncReqMgr(m_control, media_device, usersid_of_syncreqmgr));

  /* update attributes */
  m_device_tag = scen_info->device_tag;
  m_pipe_tag = pipe_tag;
  m_media_device_index = media_device;

  return MTRUE;
}

MBOOL V4L2PipeMgr::queryV4l2StreamNode(
    std::vector<std::shared_ptr<v4l2::V4L2StreamNode> >* v_node) const {
  if (mv_active_node.empty()) {
    MY_LOGE("stream node is not created yet, query fail!");
    return MFALSE;
  } else if (!v_node || !v_node->empty()) {
    MY_LOGE("destination node is null or not empty");
    return MFALSE;
  } else {
    v_node->assign(mv_active_node.begin(), mv_active_node.end());
  }

  MY_LOGD("Get steam node ok, sensor index: %d, pipe tag: %d", m_sensor_idx,
          m_pipe_tag);
  return MTRUE;
}

MBOOL V4L2PipeMgr::queryMediaEntity(
    std::vector<std::shared_ptr<MediaEntity> >* v_media_entity) const {
  if (mv_media_entity.empty()) {
    MY_LOGE("media entity is not created yet, query fail!");
    return MFALSE;
  } else if (!v_media_entity || !v_media_entity->empty()) {
    MY_LOGE("query media entity container is null or not empty");
    return MFALSE;
  } else {
    v_media_entity->assign(mv_media_entity.begin(), mv_media_entity.end());
  }

  MY_LOGD("Get media entity ok, sensor index: %d, pipe tag: %d", m_sensor_idx,
          m_pipe_tag);
  return MTRUE;
}

status_t V4L2PipeMgr::enableLink(DynamicLinkTag tag, const char* dev_name) {
  return m_control->enableLink(m_media_device_index, tag, dev_name);
}

status_t V4L2PipeMgr::disableLink(DynamicLinkTag tag, const char* dev_name) {
  return m_control->disableLink(m_media_device_index, tag, dev_name);
}
