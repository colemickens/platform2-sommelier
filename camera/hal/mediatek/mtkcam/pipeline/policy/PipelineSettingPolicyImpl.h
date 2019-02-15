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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_PIPELINESETTINGPOLICYIMPL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_PIPELINESETTINGPOLICYIMPL_H_
//
#include <memory>
#include <mtkcam/pipeline/policy/InterfaceTableDef.h>
#include <mtkcam/pipeline/policy/IPipelineSettingPolicy.h>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace pipelinesetting {

/******************************************************************************
 *
 ******************************************************************************/
class PipelineSettingPolicyImpl : public IPipelineSettingPolicy {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////    Static data (unchangable)
  std::shared_ptr<PipelineStaticInfo const> mPipelineStaticInfo;
  std::shared_ptr<PipelineUserConfiguration const> mPipelineUserConfiguration;
  std::shared_ptr<PolicyTable const> mPolicyTable;
  std::shared_ptr<MediatorTable const> mMediatorTable;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Instantiation.
  /**
   * A structure for creation parameters.
   */
  struct CreationParams {
    std::shared_ptr<PipelineStaticInfo const> pPipelineStaticInfo;
    std::shared_ptr<PipelineUserConfiguration const> pPipelineUserConfiguration;
    std::shared_ptr<PolicyTable> pPolicyTable;
    std::shared_ptr<MediatorTable> pMediatorTable;
  };
  explicit PipelineSettingPolicyImpl(CreationParams const& creationParams);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IPipelineSettingPolicy Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Interfaces.
  auto decideConfiguredAppImageStreamMaxBufNum(
      ParsedAppImageStreamInfo* pInOut,
      StreamingFeatureSetting const* pStreamingFeatureSetting,
      CaptureFeatureSetting const* pCaptureFeatureSetting) -> int override;

  auto evaluateConfiguration(ConfigurationOutputParams* out,
                             ConfigurationInputParams const& in)
      -> int override;

  auto evaluateRequest(RequestOutputParams* out __unused,
                       RequestInputParams const& in __unused) -> int override;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace pipelinesetting
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_PIPELINESETTINGPOLICYIMPL_H_
