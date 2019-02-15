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

#define LOG_TAG "mtkcam-P1DmaNeedPolicy"

#include <camera_custom_eis.h>
#include <mtkcam/feature/3dnr/3dnr_defs.h>
#include <mtkcam/pipeline/policy/IP1DmaNeedPolicy.h>
#include <mtkcam/utils/LogicalCam/IHalLogicalDeviceList.h>
//
#include "MyUtils.h"
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

bool p1DmaOutputQuery(bool* needLcso,
                      bool* needRsso,
                      StreamingFeatureSetting const* pStreamingFeatureSetting) {
  *needLcso = ::property_get_int32("vendor.debug.enable.lcs", 1);
  MY_LOGD("needLcso:%d", *needLcso);
  // RSSO
  *needRsso = false;
  return true;
}

/**
 * Make a function target - default version
 */
FunctionType_Configuration_P1DmaNeedPolicy
makePolicy_Configuration_P1DmaNeed_Default() {
  return [](std::vector<uint32_t>* pvOut,
            std::vector<P1HwSetting> const* pP1HwSetting,
            StreamingFeatureSetting const* pStreamingFeatureSetting,
            PipelineStaticInfo const* pPipelineStaticInfo,
            PipelineUserConfiguration const* pPipelineUserConfiguration
                __unused) -> int {
    bool needLcso, needRsso;
    p1DmaOutputQuery(&needLcso, &needRsso, pStreamingFeatureSetting);
    //
    for (size_t idx = 0; idx < pPipelineStaticInfo->sensorIds.size(); idx++) {
      uint32_t setting = 0;

      if ((*pP1HwSetting)[idx].imgoSize.size()) {
        setting |= P1_IMGO;
      }
      if ((*pP1HwSetting)[idx].rrzoSize.size()) {
        setting |= P1_RRZO;
      }
      if (needLcso) {
        setting |= P1_LCSO;
      }
      if (needRsso) {
        setting |= P1_RSSO;
      }

      pvOut->push_back(setting);
    }
    return OK;
  };
}

/**
 * Make a function target - multi-cam version
 */
FunctionType_Configuration_P1DmaNeedPolicy
makePolicy_Configuration_P1DmaNeed_MultiCam() {
  return [](std::vector<uint32_t>* pvOut,
            std::vector<P1HwSetting> const* pP1HwSetting,
            StreamingFeatureSetting const* pStreamingFeatureSetting,
            PipelineStaticInfo const* pPipelineStaticInfo,
            PipelineUserConfiguration const* pPipelineUserConfiguration
                __unused) -> int {
    bool needLcso, needRsso;
    p1DmaOutputQuery(&needLcso, &needRsso, pStreamingFeatureSetting);
    //
    for (size_t idx = 0; idx < pPipelineStaticInfo->sensorIds.size(); idx++) {
      uint32_t setting = 0;

      if ((*pP1HwSetting)[idx].imgoSize.size()) {
        setting |= P1_IMGO;
      }
      // if usingCamSV is true, it cannot set rrzo.
      if ((*pP1HwSetting)[idx].rrzoSize.size() &&
          !(*pP1HwSetting)[idx].usingCamSV) {
        setting |= P1_RRZO;
      }
      if (needLcso) {
        setting |= P1_LCSO;
      }
      if (needRsso) {
        setting |= P1_RSSO;
      }

      pvOut->push_back(setting);
    }
    return OK;
  };
}

};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
