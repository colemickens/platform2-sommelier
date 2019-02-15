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

#define LOG_TAG "mtkcam-PipelineSettingPolicy"

#include "mtkcam/pipeline/policy/PipelineSettingPolicyImpl.h"

#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/
// using namespace NSCam;
// using namespace NSCam::v3::pipeline::policy::pipelinesetting;

/******************************************************************************
 *
 ******************************************************************************/
#define TRY_INVOKE_WITH_ERROR_RETURN(_policy_table_, _function_, ...) \
  do {                                                                \
    if (CC_UNLIKELY(_policy_table_ == nullptr)) {                     \
      MY_LOGE("bad policy table");                                    \
      return -ENODEV;                                                 \
    }                                                                 \
    if (CC_UNLIKELY(_policy_table_->_function_ == nullptr)) {         \
      MY_LOGE("bad policy table");                                    \
      return -ENOSYS;                                                 \
    }                                                                 \
    auto err = _policy_table_->_function_(__VA_ARGS__);               \
    if (CC_UNLIKELY(0 != err)) {                                      \
      MY_LOGE("err:%d(%s)", err, ::strerror(-err));                   \
      return err;                                                     \
    }                                                                 \
  } while (0)

/******************************************************************************
 *
 ******************************************************************************/
NSCam::v3::pipeline::policy::pipelinesetting::PipelineSettingPolicyImpl::
    PipelineSettingPolicyImpl(CreationParams const& creationParams)
    : IPipelineSettingPolicy(),
      mPipelineStaticInfo(creationParams.pPipelineStaticInfo),
      mPipelineUserConfiguration(creationParams.pPipelineUserConfiguration),
      mPolicyTable(creationParams.pPolicyTable),
      mMediatorTable(creationParams.pMediatorTable) {}

/******************************************************************************
 *
 ******************************************************************************/
auto NSCam::v3::pipeline::policy::pipelinesetting::PipelineSettingPolicyImpl::
    decideConfiguredAppImageStreamMaxBufNum(
        ParsedAppImageStreamInfo* pInOut __unused,
        StreamingFeatureSetting const* pStreamingFeatureSetting __unused,
        CaptureFeatureSetting const* pCaptureFeatureSetting __unused) -> int {
  TRY_INVOKE_WITH_ERROR_RETURN(
      mPolicyTable, fConfigStreamInfo_AppImageStreamInfoMaxBufNum, pInOut,
      pStreamingFeatureSetting, pCaptureFeatureSetting,
      mPipelineStaticInfo.get(), mPipelineUserConfiguration.get());
  return 0;  // OK
}

/******************************************************************************
 *
 ******************************************************************************/
auto NSCam::v3::pipeline::policy::pipelinesetting::PipelineSettingPolicyImpl::
    evaluateConfiguration(ConfigurationOutputParams* out __unused,
                          ConfigurationInputParams const& in __unused) -> int {
  auto const pMediator = mMediatorTable->pConfigSettingPolicyMediator;
  if (CC_UNLIKELY(pMediator == nullptr)) {
    MY_LOGE("Bad pConfigSettingPolicyMediator");
    return -ENODEV;
  }

  return pMediator->evaluateConfiguration(out, in);
}

/******************************************************************************
 *
 ******************************************************************************/
auto NSCam::v3::pipeline::policy::pipelinesetting::PipelineSettingPolicyImpl::
    evaluateRequest(RequestOutputParams* out __unused,
                    RequestInputParams const& in __unused) -> int {
  auto const pMediator = mMediatorTable->pRequestSettingPolicyMediator;
  if (CC_UNLIKELY(pMediator == nullptr)) {
    MY_LOGE("Bad pRequestSettingPolicyMediator");
    return -ENODEV;
  }

  return pMediator->evaluateRequest(out, in);
}
