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

#define LOG_TAG "mtkcam-ConfigSettingPolicyMediator"

#include <memory>
#include <mtkcam/pipeline/policy/IConfigSettingPolicyMediator.h>
#include <mtkcam/pipeline/policy/InterfaceTableDef.h>

#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/

/******************************************************************************
 *
 ******************************************************************************/
class ConfigSettingPolicyMediator_Default
    : public NSCam::v3::pipeline::policy::pipelinesetting::
          IConfigSettingPolicyMediator {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////    Static data (unchangable)
  std::shared_ptr<NSCam::v3::pipeline::policy::PipelineStaticInfo const>
      mPipelineStaticInfo;
  std::shared_ptr<NSCam::v3::pipeline::policy::PipelineUserConfiguration const>
      mPipelineUserConfiguration;
  std::shared_ptr<
      NSCam::v3::pipeline::policy::pipelinesetting::PolicyTable const>
      mPolicyTable;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Instantiation.
  explicit ConfigSettingPolicyMediator_Default(
      NSCam::v3::pipeline::policy::pipelinesetting::
          MediatorCreationParams const& params);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  IConfigSettingPolicyMediator Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////    Interfaces.
  auto evaluateConfiguration(
      NSCam::v3::pipeline::policy::pipelinesetting::ConfigurationOutputParams*
          out,
      NSCam::v3::pipeline::policy::pipelinesetting::
          ConfigurationInputParams const& in) -> int override;
};

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<
    NSCam::v3::pipeline::policy::pipelinesetting::IConfigSettingPolicyMediator>
makeConfigSettingPolicyMediator_Default(
    NSCam::v3::pipeline::policy::pipelinesetting::MediatorCreationParams const&
        params) {
  return std::make_shared<ConfigSettingPolicyMediator_Default>(params);
}

/******************************************************************************
 *
 ******************************************************************************/
ConfigSettingPolicyMediator_Default::ConfigSettingPolicyMediator_Default(
    NSCam::v3::pipeline::policy::pipelinesetting::MediatorCreationParams const&
        params)
    : IConfigSettingPolicyMediator(),
      mPipelineStaticInfo(params.pPipelineStaticInfo),
      mPipelineUserConfiguration(params.pPipelineUserConfiguration),
      mPolicyTable(params.pPolicyTable) {}

/******************************************************************************
 *
 ******************************************************************************/
auto ConfigSettingPolicyMediator_Default::evaluateConfiguration(
    NSCam::v3::pipeline::policy::pipelinesetting::ConfigurationOutputParams*
        out,
    NSCam::v3::pipeline::policy::pipelinesetting::
        ConfigurationInputParams const& in __unused) -> int {
  //---------------------------------
  // 1st level

  NSCam::v3::pipeline::policy::featuresetting::ConfigurationInputParams
      featureIn;
  NSCam::v3::pipeline::policy::featuresetting::ConfigurationOutputParams
      featureOut;
  featureIn.pSessionParams =
      &mPipelineUserConfiguration->pParsedAppConfiguration->sessionParams;

  // check zsl enable tag in config stage
  MUINT8 bConfigEnableZsl = MFALSE;
  if (NSCam::IMetadata::getEntry<MUINT8>(featureIn.pSessionParams,
                                         MTK_CONTROL_ENABLE_ZSL,
                                         &bConfigEnableZsl)) {
    MY_LOGD("Get ZSL enable in config meta (%d) : %d", MTK_CONTROL_ENABLE_ZSL,
            bConfigEnableZsl);
  }
  featureIn.isZslMode = (bConfigEnableZsl) ? true : false;
  // VR mode do not to enable ZSL
  if (mPipelineUserConfiguration->pParsedAppImageStreamInfo->hasVideoConsumer) {
    MY_LOGD("Force to disable ZSL in VR");
    featureIn.isZslMode = false;
  }
  if ((mPipelineUserConfiguration->pParsedAppImageStreamInfo
           ->pAppImage_Input_Yuv) ||
      (mPipelineUserConfiguration->pParsedAppImageStreamInfo
           ->pAppImage_Output_Priv) ||
      (mPipelineUserConfiguration->pParsedAppImageStreamInfo
           ->pAppImage_Input_Priv)) {
    MY_LOGD("Force to disable ZSL in reprocessing mode");
    featureIn.isZslMode = false;
  }

  RETURN_IF_ERROR(mPolicyTable->mFeaturePolicy->evaluateConfiguration(
                      &featureOut, &featureIn),
                  "mFeaturePolicy->evaluateConfiguration");

  *(out->pStreamingFeatureSetting) = featureOut.StreamingParams;
  *(out->pCaptureFeatureSetting) = featureOut.CaptureParams;
  if (out->pIsZSLMode != nullptr) {
    *(out->pIsZSLMode) = featureIn.isZslMode;
  }

  RETURN_IF_ERROR(
      mPolicyTable->fConfigPipelineNodesNeed(
          NSCam::v3::pipeline::policy::Configuration_PipelineNodesNeed_Params{
              .pOut = out->pPipelineNodesNeed,
              .pPipelineStaticInfo = mPipelineStaticInfo.get(),
              .pPipelineUserConfiguration = mPipelineUserConfiguration.get()}),
      "fConfigPipelineNodesNeed");

  //---------------------------------
  // 2nd level

  if (!in.bypassSensorSetting) {
    RETURN_IF_ERROR(
        mPolicyTable->fSensorSetting(
            out->pSensorSetting, out->pStreamingFeatureSetting,
            mPipelineStaticInfo.get(), mPipelineUserConfiguration.get()),
        "fSensorSetting");
  }

  //---------------------------------
  // 3rd level

  RETURN_IF_ERROR(
      mPolicyTable->fConfigP1HwSetting(
          out->pP1HwSetting, out->pSensorSetting, out->pStreamingFeatureSetting,
          out->pPipelineNodesNeed, mPipelineStaticInfo.get(),
          mPipelineUserConfiguration.get()),
      "fConfigP1HwSetting");

  RETURN_IF_ERROR(
      mPolicyTable->fConfigP1DmaNeed(
          out->pP1DmaNeed, out->pP1HwSetting, out->pStreamingFeatureSetting,
          mPipelineStaticInfo.get(), mPipelineUserConfiguration.get()),
      "fConfigP1DmaNeed");

  RETURN_IF_ERROR(
      mPolicyTable->fConfigStreamInfo_P1(
          NSCam::v3::pipeline::policy::Configuration_StreamInfo_P1_Params{
              .pvOut = out->pParsedStreamInfo_P1,
              .pvP1HwSetting = out->pP1HwSetting,
              .pvP1DmaNeed = out->pP1DmaNeed,
              .pPipelineNodesNeed = out->pPipelineNodesNeed,
              .pCaptureFeatureSetting = out->pCaptureFeatureSetting,
              .pPipelineStaticInfo = mPipelineStaticInfo.get(),
              .pPipelineUserConfiguration = mPipelineUserConfiguration.get()}),
      "fConfigStreamInfo_P1");

  //---------------------------------
  // 4th level

  RETURN_IF_ERROR(
      mPolicyTable->fConfigStreamInfo_NonP1(
          NSCam::v3::pipeline::policy::Configuration_StreamInfo_NonP1_Params{
              .pOut = out->pParsedStreamInfo_NonP1,
              .pPipelineNodesNeed = out->pPipelineNodesNeed,
              .pCaptureFeatureSetting = out->pCaptureFeatureSetting,
              .pPipelineStaticInfo = mPipelineStaticInfo.get(),
              .pPipelineUserConfiguration = mPipelineUserConfiguration.get()}),
      "fConfigStreamInfo_NonP1");

  return NSCam::OK;
}
