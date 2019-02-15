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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_INTERFACETABLEDEF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_INTERFACETABLEDEF_H_
//
#include <memory>
// Mediator
#include <mtkcam/pipeline/policy/IConfigSettingPolicyMediator.h>
#include <mtkcam/pipeline/policy/IRequestSettingPolicyMediator.h>
//
// Policy (feature setting)
#include <mtkcam/pipeline/policy/IFeatureSettingPolicy.h>
//
// Policy (Configuration)
#include <mtkcam/pipeline/policy/IConfigPipelineNodesNeedPolicy.h>
#include <mtkcam/pipeline/policy/ISensorSettingPolicy.h>
#include <mtkcam/pipeline/policy/IP1HwSettingPolicy.h>
#include <mtkcam/pipeline/policy/IP1DmaNeedPolicy.h>
#include <mtkcam/pipeline/policy/IConfigStreamInfoPolicy.h>
#include <mtkcam/pipeline/policy/IConfigAppImageStreamInfoMaxBufNumPolicy.h>
//
// Policy (Request)
#include <mtkcam/pipeline/policy/IFaceDetectionIntentPolicy.h>
#include <mtkcam/pipeline/policy/IP2NodeDecisionPolicy.h>
#include <mtkcam/pipeline/policy/ITopologyPolicy.h>
#include <mtkcam/pipeline/policy/ICaptureStreamUpdaterPolicy.h>
#include <mtkcam/pipeline/policy/IIOMapPolicy.h>
#include <mtkcam/pipeline/policy/IRequestMetadataPolicy.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace pipelinesetting {

/**
 * Policy table definition
 */
struct PolicyTable {
  FunctionType_Configuration_PipelineNodesNeedPolicy fConfigPipelineNodesNeed =
      nullptr;
  FunctionType_SensorSettingPolicy fSensorSetting = nullptr;
  FunctionType_Configuration_P1HwSettingPolicy fConfigP1HwSetting = nullptr;
  FunctionType_Configuration_P1DmaNeedPolicy fConfigP1DmaNeed = nullptr;
  FunctionType_Configuration_StreamInfo_P1 fConfigStreamInfo_P1 = nullptr;
  FunctionType_Configuration_StreamInfo_NonP1 fConfigStreamInfo_NonP1 = nullptr;

  FunctionType_Configuration_AppImageStreamInfoMaxBufNumPolicy
      fConfigStreamInfo_AppImageStreamInfoMaxBufNum = nullptr;

  FunctionType_FaceDetectionIntentPolicy fFaceDetectionIntent = nullptr;
  FunctionType_P2NodeDecisionPolicy fP2NodeDecision = nullptr;
  FunctionType_TopologyPolicy fTopology = nullptr;
  FunctionType_CaptureStreamUpdaterPolicy fCaptureStreamUpdater = nullptr;
  FunctionType_IOMapPolicy_P2Node fIOMap_P2Node = nullptr;
  FunctionType_IOMapPolicy_NonP2Node fIOMap_NonP2Node = nullptr;
  std::shared_ptr<requestmetadata::IRequestMetadataPolicy>
      pRequestMetadataPolicy = nullptr;

  /* for feature policy */
  std::shared_ptr<featuresetting::IFeatureSettingPolicy> mFeaturePolicy =
      nullptr;
};

/**
 * Mediator table definition
 */
struct MediatorTable {
  std::shared_ptr<IConfigSettingPolicyMediator> pConfigSettingPolicyMediator =
      nullptr;
  std::shared_ptr<IRequestSettingPolicyMediator> pRequestSettingPolicyMediator =
      nullptr;
};

////////////////////////////////////////////////////////////////////////////////

/**
 * Mediator Creation related Parameters
 */
struct MediatorCreationParams {
  std::shared_ptr<PipelineStaticInfo const> pPipelineStaticInfo;
  std::shared_ptr<PipelineUserConfiguration const> pPipelineUserConfiguration;
  std::shared_ptr<PolicyTable> pPolicyTable;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace pipelinesetting
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_INTERFACETABLEDEF_H_
