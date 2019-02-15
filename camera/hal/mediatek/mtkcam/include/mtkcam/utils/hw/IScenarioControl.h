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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_ISCENARIOCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_ISCENARIOCONTROL_H_
//
#include <memory>
#include <mtkcam/def/common.h>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

#define FEATURE_CFG_ENABLE_MASK(x, y) (x) |= (1 << (y))
#define FEATURE_CFG_IS_ENABLED(x, y) (((x) & (1 << (y))) ? MTRUE : MFALSE)

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Bandwidth control & dvfs
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class IScenarioControl {
 public:
  enum {
    Scenario_None,
    Scenario_NormalPreivew,
    Scenario_ZsdPreview,
    Scenario_VideoRecord,
    Scenario_VSS,
    Scenario_Capture,
    Scenario_ContinuousShot,
    Scenario_StreamingWithJpeg = Scenario_ContinuousShot,  // hal3
    Scenario_VideoTelephony,
    Scenario_HighSpeedVideo,
  };

  enum {
    FEATURE_NONE = 0,
    FEATURE_IVHDR,
    FEATURE_MVHDR,
    FEATURE_ZVHDR,
    FEATURE_VFB,
  };

  struct ControlParam {
    MINT32 scenario;
    MSize sensorSize;
    MINT32 sensorFps;
    MINT32 featureFlag;
    MBOOL enableBWCControl;
    MSize videoSize;
    MINT32 camMode;
    MINT32 supportCustomOption;
    ControlParam()
        : scenario(Scenario_None),
          sensorSize(MSize(0, 0)),
          sensorFps(0),
          featureFlag(FEATURE_NONE),
          enableBWCControl(MTRUE),
          videoSize(MSize(0, 0)),
          camMode(0),
          supportCustomOption(0) {}
  };

 public:
  static std::shared_ptr<IScenarioControl> create(MINT32 const openId);
  virtual ~IScenarioControl() {}

 public:
  virtual MERROR enterScenario(ControlParam const& param) = 0;

  virtual MERROR enterScenario(MINT32 const scenario) = 0;

  virtual MERROR exitScenario() = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_HW_ISCENARIOCONTROL_H_
