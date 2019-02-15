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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ISENSORSETTINGPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ISENSORSETTINGPOLICY_H_
//
#include "./types.h"

#include <functional>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

/**
 * The function type definition.
 * It is used to decide sensor settings.
 *
 * @param[out] pvOut: sensor settings
 * For example,
 *      (*pvOut)[0] and (*pvOut)[1] are the sensor settings
 *      for sensorId[0] and sensorId[1] respectively.
 *
 * @param[in] pStreamingFeatureSetting: the streaming feature settings.
 *
 * @param[in] PipelineStaticInfo: Pipeline static information.
 *
 * @param[in] PipelineUserConfiguration: Pipeline user configuration.
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
using FunctionType_SensorSettingPolicy = std::function<int(
    std::vector<SensorSetting>* pvOut,
    StreamingFeatureSetting const* pStreamingFeatureSetting,
    PipelineStaticInfo const* pPipelineStaticInfo,
    PipelineUserConfiguration const* pPipelineUserConfiguration)>;

//==============================================================================

/**
 * Policy instance makers
 *
 */

// default version
FunctionType_SensorSettingPolicy makePolicy_SensorSetting_Default();

// slow motion (SMVR)
FunctionType_SensorSettingPolicy makePolicy_SensorSetting_SMVR();

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ISENSORSETTINGPOLICY_H_
