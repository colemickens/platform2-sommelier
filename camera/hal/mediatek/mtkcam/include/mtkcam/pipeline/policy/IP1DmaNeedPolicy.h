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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IP1DMANEEDPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IP1DMANEEDPOLICY_H_
//
#include <mtkcam/pipeline/policy/types.h>

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
 * It is used to decide P1 DMA need at the configuration stage.
 *
 * @param[out] pvOut: P1 DMA need
 * The value shows which dma are needed.
 * For example,
 *      ((*pvOut)[0] & P1_IMGO) != 0 indicates that IMGO is needed for
 * sensorId[0].
 *      ((*pvOut)[1] & P1_RRZO) != 0 indicates that RRZO is needed for
 * sensorId[1].
 *
 * @param[in] pP1HwSetting: the P1 hardware settings
 *
 * @param[in] pSensorSetting: the sensor settings
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
using FunctionType_Configuration_P1DmaNeedPolicy = std::function<int(
    std::vector<uint32_t>* /*pvOut*/,
    std::vector<P1HwSetting> const* /*pP1HwSetting*/,
    StreamingFeatureSetting const* /*pStreamingFeatureSetting*/,
    PipelineStaticInfo const* /*pPipelineStaticInfo*/,
    PipelineUserConfiguration const* /*pPipelineUserConfiguration*/)>;

//==============================================================================

/**
 * Policy instance makers
 *
 */

// default version
FunctionType_Configuration_P1DmaNeedPolicy
makePolicy_Configuration_P1DmaNeed_Default();

// multi-cam version
FunctionType_Configuration_P1DmaNeedPolicy
makePolicy_Configuration_P1DmaNeed_MultiCam();

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IP1DMANEEDPOLICY_H_
