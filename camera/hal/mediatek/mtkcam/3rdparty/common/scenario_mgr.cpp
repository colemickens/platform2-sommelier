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

#define LOG_TAG "mtkcam-scenario_mgr"
//
#include <mtkcam/3rdparty/common/scenario_mgr.h>
#include <mtkcam/3rdparty/customer/customer_scenario_mgr.h>
#include <mtkcam/3rdparty/mtk/mtk_scenario_mgr.h>
#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/std/Trace.h>

namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace scenariomgr {

auto get_capture_scenario(
    int32_t* pScenario, /*eCameraScenarioCustomer or eCameraScenarioMtk*/
    const ScenarioHint& scenarioHint,
    IMetadata const* pAppMetadata) -> bool {
  return customer_get_capture_scenario(pScenario, scenarioHint, pAppMetadata) ||
         mtk_get_capture_scenario(pScenario, scenarioHint, pAppMetadata);
}

auto get_streaming_scenario(
    int32_t* pScenario, /*eCameraScenarioCustomer or eCameraScenarioMtk*/
    const ScenarioHint& scenarioHint,
    IMetadata const* pAppMetadata) -> bool {
  return customer_get_streaming_scenario(pScenario, scenarioHint,
                                         pAppMetadata) ||
         mtk_get_streaming_scenario(pScenario, scenarioHint, pAppMetadata);
}

auto get_features_table_by_scenario(
    int32_t openId,
    int32_t scenario, /*eCameraScenarioCustomer or eCameraScenarioMtk*/
    ScenarioFeatures* pScenarioFeatures) -> bool {
  return customer_get_features_table_by_scenario(openId, scenario,
                                                 pScenarioFeatures) ||
         mtk_get_features_table_by_scenario(openId, scenario,
                                            pScenarioFeatures);
}

};  // namespace scenariomgr
};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
