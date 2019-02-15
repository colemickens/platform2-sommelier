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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2TUNINGPIPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2TUNINGPIPE_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "V4L2IHalCamIO.h"
#include "V4L2IIOPipe.h"
#include "V4L2PipeBase.h"

namespace NSCam {
namespace NSIoPipe {
namespace NSCamIOPipe {

class V4L2TuningPipe : public V4L2PipeBase {
 public:
  const int LISTENED_NODE_ID = v4l2::V4L2StreamNode::ID_P1_TUNING;

  /**
   * Create a Statistic pipe.
   */
  V4L2TuningPipe(IspPipeType pipe_type,
                 MUINT32 sensor_idx,
                 const char* sz_caller_name);

  V4L2TuningPipe(const V4L2TuningPipe&) = delete;
  V4L2TuningPipe& operator=(const V4L2TuningPipe&) = delete;
  ~V4L2TuningPipe() = default;

  MBOOL init(const PipeTag pipe_tag) override;
  MBOOL uninit() override;
  MBOOL configPipe(const QInitParam& init_param,
                   std::map<int, std::vector<std::shared_ptr<IImageBuffer> > >*
                       map_vbuffers) override;

 private:
  /* saves node name. e.g.: meta1, or meta2 */
  std::string m_node_name;
};

}  // namespace NSCamIOPipe
}  // namespace NSIoPipe
}  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_PASS1_INC_V4L2TUNINGPIPE_H_
