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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_REQUEST_REQUESTMETADATAPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_REQUEST_REQUESTMETADATAPOLICY_H_
//
#include <mtkcam/pipeline/policy/IRequestMetadataPolicy.h>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace requestmetadata {

/******************************************************************************
 *
 ******************************************************************************/
class RequestMetadataPolicy_Default : public IRequestMetadataPolicy {
 public:
  virtual auto evaluateRequest(EvaluateRequestParams const& params) -> int;

 public:
  // RequestMetadataPolicy Interfaces.
  explicit RequestMetadataPolicy_Default(CreationParams const& params);

 private:
  CreationParams mPolicyParams;
  std::vector<MSize> mvTargetRrzoSize;
};

/******************************************************************************
 *
 ******************************************************************************/
class RequestMetadataPolicy_DebugDump : public IRequestMetadataPolicy {
 public:
  virtual auto evaluateRequest(EvaluateRequestParams const& params) -> int;

 public:
  // RequestMetadataPolicy Interfaces.
  explicit RequestMetadataPolicy_DebugDump(CreationParams const& params);

 private:
  CreationParams mPolicyParams;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace requestmetadata
};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_REQUEST_REQUESTMETADATAPOLICY_H_
