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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICONFIGPIPELINENODESNEEDPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICONFIGPIPELINENODESNEEDPOLICY_H_
//
#include "./types.h"

#include <functional>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

/**
 * The function type definition.
 * Decide which pipeline nodes are needed at the configuration stage.
 *
 * @param[out] PipelineNodesNeed: which pipeline nodes are needed.
 *
 * @param[in] PipelineStaticInfo: Pipeline static information.
 *
 * @param[in] PipelineUserConfiguration: Pipeline user configuration.
 *
 * @return
 *      0 indicates success; otherwise failure.
 */

struct Configuration_PipelineNodesNeed_Params {
  PipelineNodesNeed* pOut = nullptr;
  PipelineStaticInfo const* pPipelineStaticInfo = nullptr;
  PipelineUserConfiguration const* pPipelineUserConfiguration = nullptr;
};
using FunctionType_Configuration_PipelineNodesNeedPolicy = std::function<int(
    Configuration_PipelineNodesNeed_Params const& /*params*/)>;

//==============================================================================

/**
 * Policy instance makers
 *
 */

// default version
FunctionType_Configuration_PipelineNodesNeedPolicy
makePolicy_Configuration_PipelineNodesNeed_Default();
// multicam version
FunctionType_Configuration_PipelineNodesNeedPolicy
makePolicy_Configuration_PipelineNodesNeed_multicam();

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICONFIGPIPELINENODESNEEDPOLICY_H_
