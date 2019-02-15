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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2PIPEMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2PIPEMGR_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "V4L2IHalCamIO.h"
#include "MediaCtrlConfig.h"
#include "MtkCameraV4L2API.h"
#include "V4L2StreamNode.h"
#include "SyncReqMgr.h"

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

using NSCam::v4l2::SyncReqMgr;

class V4L2PipeFactory;

class V4L2PipeMgr {
  friend class V4L2PipeFactory;

 public:
  V4L2PipeMgr(const V4L2PipeMgr&) = delete;
  V4L2PipeMgr& operator=(const V4L2PipeMgr&) = delete;
  explicit V4L2PipeMgr(MUINT32 sensor_idx);
  virtual ~V4L2PipeMgr() = default;
  MBOOL queryV4l2StreamNode(
      std::vector<std::shared_ptr<NSCam::v4l2::V4L2StreamNode>>* v_node) const;
  MBOOL queryMediaEntity(
      std::vector<std::shared_ptr<MediaEntity>>* v_media_entity) const;
  status_t enableLink(DynamicLinkTag tag, const char* dev_name);
  status_t disableLink(DynamicLinkTag tag, const char* dev_name);

  std::shared_ptr<V4L2VideoNode> getSubDev() const { return m_p1_subdev; }
  // get sync request manager
  const SyncReqMgr* getSyncRegMgr() const { return m_syncReqMgr.get(); }
  SyncReqMgr* getSyncRegMgr() { return m_syncReqMgr.get(); }
  // query index of media device of P1
  int getMediaDevIndex() { return m_media_device_index; }
  int getMediaDevIndex() const { return m_media_device_index; }

 public:
  struct ScenarioInfo {
    PipeTag pipe_tag;
    MediaDeviceTag device_tag;
    std::string pipe_name;
    std::string device_name;
  };

 public:
  const ScenarioInfo m_scenario_mapper[kPipeTag_Num] = {
      {kPipeTag_Out1, MEDIA_CONTROLLER_P1_OUT1, "preview", "p1-out1"},
      {kPipeTag_Out2, MEDIA_CONTROLLER_P1_OUT2, "preview", "p1-out2"},
      {kPipeTag_Out1_Tuning, MEDIA_CONTROLLER_P1_OUT1, "preview", "p1-out1"},
      {kPipeTag_Out2_Tuning, MEDIA_CONTROLLER_P1_OUT2, "preview", "p1-out2"},
  };

  const std::unordered_map<int, const char*> m_port_mapper = {
      {NSImageio::NSIspio::EPortIndex_IMGO, "mtk-cam-p1 main stream"},
      {NSImageio::NSIspio::EPortIndex_RRZO, "mtk-cam-p1 packed out"},
      {NSImageio::NSIspio::EPortIndex_META1, "mtk-cam-p1 partial meta 0"},
      {NSImageio::NSIspio::EPortIndex_META2, "mtk-cam-p1 partial meta 1"},
      {NSImageio::NSIspio::EPortIndex_LCSO, "mtk-cam-p1 partial meta 2"},
      {NSImageio::NSIspio::EPortIndex_EISO, "mtk-cam-p1 partial meta 3"},
      {NSImageio::NSIspio::EPortIndex_TUNING, "mtk-cam-p1 meta input"},
  };

 public:
  inline PipeTag getPipeTag() const { return m_pipe_tag; }

 private:
  MBOOL buildV4l2Links(PipeTag pipe_tag);

  MediaDeviceTag m_device_tag;
  int m_media_device_index;
  PipeTag m_pipe_tag;
  std::shared_ptr<MtkCameraV4L2API> m_control;
  std::vector<std::shared_ptr<MediaEntity>> mv_media_entity;
  std::vector<std::shared_ptr<NSCam::v4l2::V4L2StreamNode>> mv_active_node;
  std::shared_ptr<V4L2VideoNode> m_p1_subdev;
  std::shared_ptr<SyncReqMgr> m_syncReqMgr;
  const int m_sensor_idx;
};

}  // end of namespace NSCamIOPipe
}  // end of namespace NSIoPipe
}  // end of namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2PIPEMGR_H_
