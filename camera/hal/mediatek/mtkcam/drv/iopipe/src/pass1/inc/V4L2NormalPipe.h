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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2NORMALPIPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2NORMALPIPE_H_

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "V4L2IHalCamIO.h"
#include "V4L2IIOPipe.h"
#include "V4L2PipeBase.h"

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

class V4L2NormalPipe : public V4L2PipeBase {
 public:
  const int LISTENED_NODE_ID = v4l2::V4L2StreamNode::ID_P1_MAIN_STREAM |
                               v4l2::V4L2StreamNode::ID_P1_SUB_STREAM |
                               v4l2::V4L2StreamNode::ID_P1_META3 |
                               v4l2::V4L2StreamNode::ID_P1_META4;

  V4L2NormalPipe(IspPipeType pipe_type,
                 MUINT32 sensor_idx,
                 const char* sz_caller_name);
  V4L2NormalPipe(const V4L2NormalPipe&) = delete;
  V4L2NormalPipe& operator=(const V4L2NormalPipe&) = delete;
  ~V4L2NormalPipe() = default;

  MBOOL init(const PipeTag pipe_tag) override;
  MBOOL uninit() override;
  MBOOL sendCommand(const int cmd,
                    MINTPTR arg1,
                    MINTPTR arg2,
                    MINTPTR arg3) override;
  MBOOL configPipe(const QInitParam& init_param,
                   std::map<int, std::vector<std::shared_ptr<IImageBuffer>>>*
                       map_vbuffers) override;

 private:
  void setRawPxlOrder(QInitParam* init_param);
  // saves actived video nodes (id should be ID_P1_MAIN_STREAM or
  // ID_P1_SUB_STREAM)
  std::unordered_map<int, std::weak_ptr<v4l2::V4L2StreamNode>> m_videoNodes;
  std::unique_ptr<IHalSensor, std::function<void(IHalSensor*)>> mup_halsensor;
};

}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2NORMALPIPE_H_
