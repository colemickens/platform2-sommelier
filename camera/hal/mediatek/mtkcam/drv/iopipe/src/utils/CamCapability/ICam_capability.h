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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_UTILS_CAMCAPABILITY_ICAM_CAPABILITY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_UTILS_CAMCAPABILITY_ICAM_CAPABILITY_H_

#include <mtkcam/def/ImageFormat.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>
#include <string>
#include <vector>

using std::vector;

typedef enum {
  ECamio_unknown = 0x0,
  ECamio_valid_fmt = 0x1,
  ECamio_BS_ratio = 0x2,
  ECamio_pipeline_bitdepth = 0x4,
  ECamio_sen_num = 0x8,
  ECamio_pipeline_d_twin = 0x10,
  ECamio_pipeline_uni_num = 0x20,

  ECamio_valid_cmd =
      (ECamio_valid_fmt | ECamio_BS_ratio | ECamio_pipeline_bitdepth |
       ECamio_sen_num | ECamio_pipeline_d_twin | ECamio_pipeline_uni_num),
} E_CAM_Query;

struct CAM_Queryrst {
  MUINT32 ratio;  // unit:%
  vector<NSCam::EImageFormat> Queue_fmt;
  MUINT32 pipelinebitdepth;
  MUINT32 SenNum;

  CAM_Queryrst(MUINT32 _ratio = 100,
               MUINT32 _pipelinebitdepth = 1,
               MUINT32 _SenNum = 0)
      : ratio(_ratio), pipelinebitdepth(_pipelinebitdepth), SenNum(_SenNum) {}
};

class VISIBILITY_PUBLIC CAM_CAPABILITY {
 public:
  /**
   *@brief constructor
   */
  CAM_CAPABILITY();
  virtual ~CAM_CAPABILITY() = default;
  /**
   *@brief get cam_capability object
   *@return
   *-cam_capability object
   */
  static CAM_CAPABILITY* getInstance(
      std::string szCallerName,
      NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM module =
          NSCam::NSIoPipe::NSCamIOPipe::ENPipe_UNKNOWN);

  virtual MBOOL GetCapability(
      MUINT32 portId,
      NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd e_Op,
      NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_InputInfo inputInfo,
      NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo* p_QueryRst);

 private:
  NSCam::NSIoPipe::NSCamIOPipe::ENPipe_CAM m_hwModule;
  std::string m_Name;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_UTILS_CAMCAPABILITY_ICAM_CAPABILITY_H_
