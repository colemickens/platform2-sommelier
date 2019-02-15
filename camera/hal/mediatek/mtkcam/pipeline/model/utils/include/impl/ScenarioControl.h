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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_SCENARIOCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_SCENARIOCONTROL_H_
//
#include <memory>
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

#ifndef FEATURE_CFG_ENABLE_MASK
#define FEATURE_CFG_ENABLE_MASK(x, y) (x) |= (1 << (y))
#endif
#ifndef FEATURE_CFG_IS_ENABLED
#define FEATURE_CFG_IS_ENABLED(x, y) (((x) & (1 << (y))) ? MTRUE : MFALSE)
#endif
#define SCENARIO_BOOST_MASK(x, y) (x) |= (1 << (y))
#define SCENARIO_IS_ENABLED(x, y) (((x) & (1 << (y))) ? MTRUE : MFALSE)

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Bandwidth control & dvfs
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
class IScenarioControlV3 {
 public:
  enum {
    Scenario_NormalPreivew,
    Scenario_ZsdPreview,
    Scenario_VideoRecord,
    Scenario_VSS,
    Scenario_Capture,
    Scenario_ContinuousShot,
    Scenario_StreamingWithJpeg = Scenario_ContinuousShot,  // hal3
    Scenario_VideoTelephony,
    Scenario_HighSpeedVideo,
    Scenario_MaxScenarioNumber,
    Scenario_None = -1,
  };

  enum {
    FEATURE_NONE = 0,
    FEATURE_ADV_EIS,
    FEATURE_IVHDR,
    FEATURE_MVHDR,
    FEATURE_ZVHDR,
    FEATURE_VFB,
    FEATURE_DUAL_PD,
    FEATURE_VSDOF_PREVIEW,
    FEATURE_VSDOF_RECORD,
    FEATURE_STEREO_CAPTURE,
    FEATURE_BMDENOISE_PREVIEW,
    FEATURE_BMDENOISE_CAPTURE,
    FEATURE_BMDENOISE_MFHR_CAPTURE,
    FEATURE_DUALZOOM_PREVIEW,
    FEATURE_DUALZOOM_RECORD,
    FEATURE_DUALZOOM_FUSION_CAPTURE,
    FEATURE_ADV_EIS_4K,
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
    bool enableDramClkControl;
    MINT32 dramOPPLevel;
    ControlParam()
        : scenario(Scenario_None),
          sensorSize(MSize(0, 0)),
          sensorFps(0),
          featureFlag(FEATURE_NONE),
          enableBWCControl(MTRUE),
          videoSize(MSize(0, 0)),
          camMode(0),
          supportCustomOption(0),
          enableDramClkControl(MFALSE),
          dramOPPLevel(0) {}
  };

 public:
  static std::shared_ptr<IScenarioControlV3> create(MINT32 const openId);

 public:
  virtual MERROR enterScenario(ControlParam const& param) = 0;

  virtual MERROR enterScenario(MINT32 const scenario) = 0;

  virtual MERROR exitScenario() = 0;

  virtual MERROR boostScenario(int const scenario,
                               int const feature,
                               int64_t const frameNo) = 0;

  virtual MERROR checkIfNeedExitBoost(int64_t const frameNo,
                                      int const forceExit) = 0;
  virtual ~IScenarioControlV3() {}
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_SCENARIOCONTROL_H_
