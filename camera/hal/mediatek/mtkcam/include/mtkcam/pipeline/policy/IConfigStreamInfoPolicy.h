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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICONFIGSTREAMINFOPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICONFIGSTREAMINFOPOLICY_H_
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
 * Decide Pass1-specific stream info configuration at the configuration stage.
 *
 * @param[out] pvOut: Pass1-specific stream info
 *
 * @param[in] pvP1HwSetting: P1 hardware settings.
 *
 * @param[in] pvP1DmaNeed: P1 DMA need
 * The value shows which dma are needed.
 * For example,
 *      ((*pvP1DmaNeed)[0] & P1_IMGO) != 0 indicates that IMGO is needed for
 * sensorId[0].
 *      ((*pvP1DmaNeed)[1] & P1_RRZO) != 0 indicates that RRZO is needed for
 * sensorId[1].
 *
 * @param[in] PipelineNodesNeed: which pipeline nodes are needed.
 *
 * @param[in] pCaptureFeatureSetting: capture feature setting
 *
 * @param[in] PipelineStaticInfo: Pipeline static information.
 *
 * @param[in] PipelineUserConfiguration: Pipeline user configuration.
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
struct Configuration_StreamInfo_P1_Params {
  std::vector<ParsedStreamInfo_P1>* pvOut = nullptr;
  std::vector<P1HwSetting> const* pvP1HwSetting = nullptr;
  std::vector<uint32_t> const* pvP1DmaNeed = nullptr;
  PipelineNodesNeed const* pPipelineNodesNeed = nullptr;
  CaptureFeatureSetting const* pCaptureFeatureSetting = nullptr;
  PipelineStaticInfo const* pPipelineStaticInfo = nullptr;
  PipelineUserConfiguration const* pPipelineUserConfiguration = nullptr;
};
using FunctionType_Configuration_StreamInfo_P1 =
    std::function<int(Configuration_StreamInfo_P1_Params const& /*params*/)>;

/**
 * The function type definition.
 * Decide non-Pass1-specific stream info configuration at the configuration
 * stage.
 *
 * @param[out] pOut: non-Pass1-specific stream info
 *
 * @param[in] PipelineNodesNeed: which pipeline nodes are needed.
 *
 * @param[in] pCaptureFeatureSetting: capture feature setting
 *
 * @param[in] PipelineStaticInfo: Pipeline static information.
 *
 * @param[in] PipelineUserConfiguration: Pipeline user configuration.
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
struct Configuration_StreamInfo_NonP1_Params {
  ParsedStreamInfo_NonP1* pOut = nullptr;
  PipelineNodesNeed const* pPipelineNodesNeed = nullptr;
  CaptureFeatureSetting const* pCaptureFeatureSetting = nullptr;
  PipelineStaticInfo const* pPipelineStaticInfo = nullptr;
  PipelineUserConfiguration const* pPipelineUserConfiguration = nullptr;
};
using FunctionType_Configuration_StreamInfo_NonP1 =
    std::function<int(Configuration_StreamInfo_NonP1_Params const& /*params*/)>;

//==============================================================================

/**
 * Policy instance makers
 *
 */

// default version
FunctionType_Configuration_StreamInfo_P1
makePolicy_Configuration_StreamInfo_P1_Default();

// SMVR version
FunctionType_Configuration_StreamInfo_P1
makePolicy_Configuration_StreamInfo_P1_SMVR();

// default version
FunctionType_Configuration_StreamInfo_NonP1
makePolicy_Configuration_StreamInfo_NonP1_Default();

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICONFIGSTREAMINFOPOLICY_H_
