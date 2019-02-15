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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICONFIGAPPIMAGESTREAMINFOMAXBUFNUMPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICONFIGAPPIMAGESTREAMINFOMAXBUFNUMPOLICY_H_
//
#include <functional>
#include <mtkcam/pipeline/policy/types.h>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

/**
 * The function type definition.
 * It is used to decide the maximum buffer number of app image stream info
 * at the configuration stage.
 *
 * @param[in/out] pInOut
 *  Before this call, callers must promise each App image stream info instance.
 *  On this call, each App image stream info's 'setMaxBufNum()'
 *  must be called to set up its maximum buffer number.
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
using FunctionType_Configuration_AppImageStreamInfoMaxBufNumPolicy =
    std::function<int(
        ParsedAppImageStreamInfo* /*pInOut*/,
        StreamingFeatureSetting const* /*pStreamingFeatureSetting*/,
        CaptureFeatureSetting const* /*pCaptureFeatureSetting*/,
        PipelineStaticInfo const* /*pPipelineStaticInfo*/,
        PipelineUserConfiguration const* /*pPipelineUserConfiguration*/)>;

//==============================================================================

/**
 * Policy instance makers
 *
 */

// default version
FunctionType_Configuration_AppImageStreamInfoMaxBufNumPolicy
makePolicy_Configuration_AppImageStreamInfoMaxBufNum_Default();

// SMVR version
FunctionType_Configuration_AppImageStreamInfoMaxBufNumPolicy
makePolicy_Configuration_AppImageStreamInfoMaxBufNum_SMVR();

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICONFIGAPPIMAGESTREAMINFOMAXBUFNUMPOLICY_H_
