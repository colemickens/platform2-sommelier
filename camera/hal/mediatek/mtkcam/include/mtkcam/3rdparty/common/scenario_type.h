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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_COMMON_SCENARIO_TYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_COMMON_SCENARIO_TYPE_H_
//
#include <mtkcam/3rdparty/customer/customer_feature_type.h>
#include <mtkcam/3rdparty/customer/customer_scenario_type.h>
#include <mtkcam/3rdparty/mtk/mtk_feature_type.h>
#include <mtkcam/3rdparty/mtk/mtk_scenario_type.h>
#include <mtkcam/def/common.h>
//
#include <string>
#include <vector>

// add feature set by camera scenario
#define CAMERA_SCENARIO_START(SCENARIO_NAME) \
  {                                          \
    SCENARIO_NAME, {                         \
#SCENARIO_NAME, {
#define ADD_CAMERA_FEATURE_SET(FEATURE, FEATURE_COMBINATION) \
  {#FEATURE, #FEATURE_COMBINATION, FEATURE, FEATURE_COMBINATION},

#define CAMERA_SCENARIO_END \
  }                         \
  }                         \
  }                         \
  ,

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace scenariomgr {

struct ScenarioHint {
  int32_t captureScenarioIndex =
      -1;  // force to get indicated scenario by vendor tag

  // hint for streaming scenario
  // TODO(MTK): add others hint to choose streaming scenario
  int32_t streamingScenarioIndex =
      -1;  // force to get indicated scenario by vendor tag
};

struct FeatureSet {
  std::string featureName;             // for debug
  std::string featureCombinationName;  // for debug
  MUINT64 feature = 0;
  MUINT64 featureCombination = 0;
};

struct ScenarioFeatures {
  std::string scenarioName;  // for debug
  std::vector<FeatureSet> vFeatureSet;
};

};  // namespace scenariomgr
};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_COMMON_SCENARIO_TYPE_H_
