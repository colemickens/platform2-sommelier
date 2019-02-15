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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IREQUESTMETADATAPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IREQUESTMETADATAPOLICY_H_
//
#include "./types.h"
//
#include <memory>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace requestmetadata {

/**
 * Used on the input of evaluateRequest().
 */
struct EvaluateRequestParams {
  /**
   * @param[in]
   * Request number, sent at the request stage.
   *
   */
  uint32_t requestNo = 0;

  /**
   * @param[in]
   * Request App image stream info, sent at the request stage.
   *
   */
  ParsedAppImageStreamInfo const* pRequest_AppImageStreamInfo = nullptr;

  /**
   * @param[in]
   * Request App metadata control, sent at the request stage.
   *
   */
  ParsedMetaControl const* pRequest_ParsedAppMetaControl = nullptr;

  /**
   * @param[in]
   * The current sensor setting
   *
   * pSensorSize is the pointer to an array of sensor sizes.
   * The array size is the same to the size of sensor id (i.e.
   * PipelineStaticInfo::sensorId).
   */
  std::vector<MSize> const* pSensorSize = nullptr;

  /**
   * @param[in/out] Additional metadata
   *
   * pAdditionalApp: app control metadata
   * pAdditionalHal: hal control metadata; the vector size is the same to the
   *                 size of sensor id (i.e. PipelineStaticInfo::sensorId).
   *
   * Callers must ensure they are valid instances (non nullptr).
   * Callee will append additional metadata to them if needed.
   */
  std::shared_ptr<IMetadata> pAdditionalApp = nullptr;
  std::vector<std::shared_ptr<IMetadata>> const* pvAdditionalHal = nullptr;

  bool isZSLMode = false;
  /**
   * @param[in]
   * RRZO buffer size. It is decided in configure stage
   */
  std::vector<MSize> RrzoSize;
  /**
   * @param[in]
   * the app control metadata from app request
   */
  IMetadata const* pRequest_AppControl = nullptr;
};

/**
 *
 */
class IRequestMetadataPolicy {
 public:
  virtual ~IRequestMetadataPolicy() = default;

  /**
   * The policy is in charge of reporting its requirement at the request stage.
   *
   * @param[in/out] params:
   *  Callers must ensure its content.
   *
   * @return
   *      true indicates success; otherwise failure.
   */
  virtual auto evaluateRequest(EvaluateRequestParams const& params) -> int = 0;
};

/**
 * A structure for creation parameters.
 */
struct CreationParams {
  std::shared_ptr<PipelineStaticInfo const> pPipelineStaticInfo;

  std::shared_ptr<PipelineUserConfiguration const> pPipelineUserConfiguration;

  std::shared_ptr<IRequestMetadataPolicy> pRequestMetadataPolicy;
};

/**
 * Make a policy - default version
 */
std::shared_ptr<IRequestMetadataPolicy> makePolicy_RequestMetadata_Default(
    CreationParams const& params);

/**
 * Make a policy - debug dump
 */
std::shared_ptr<IRequestMetadataPolicy> makePolicy_RequestMetadata_DebugDump(
    CreationParams const& params);

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace requestmetadata
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IREQUESTMETADATAPOLICY_H_
