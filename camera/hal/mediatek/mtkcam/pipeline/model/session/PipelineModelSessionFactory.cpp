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

#undef LOG_TAG
#define LOG_TAG "mtkcam-PipelineModelSession-Factory"
//
#include <impl/IPipelineModelSession.h>
//
#include <memory>
#include <mtkcam/pipeline/policy/IPipelineSettingPolicy.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include "PipelineModelSessionDefault.h"
#include "PipelineModelSessionStreaming.h"
#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/
using NSCam::v3::eSTREAMTYPE_IMAGE_IN;
using NSCam::v3::eSTREAMTYPE_IMAGE_INOUT;
using NSCam::v3::eSTREAMTYPE_IMAGE_OUT;
using NSCam::v3::pipeline::model::IPipelineModelSession;
using NSCam::v3::pipeline::model::IPipelineModelSessionFactory;
using NSCam::v3::pipeline::model::PipelineModelSessionBase;
using NSCam::v3::pipeline::model::PipelineModelSessionDefault;
using NSCam::v3::pipeline::model::PipelineUserConfiguration;
using NSCam::v3::pipeline::model::UserConfigurationParams;
using NSCam::v3::pipeline::policy::ParsedAppConfiguration;
using NSCam::v3::pipeline::policy::ParsedAppImageStreamInfo;
using NSCam::v3::pipeline::policy::PipelineStaticInfo;
using NSCam::v3::pipeline::policy::pipelinesetting::IPipelineSettingPolicy;
using NSCam::v3::pipeline::policy::pipelinesetting::
    IPipelineSettingPolicyFactory;
/******************************************************************************
 *
 ******************************************************************************/
static auto parseAppStreamInfo(
    std::shared_ptr<NSCam::v3::pipeline::model::PipelineUserConfiguration>
        config) -> bool {
  auto const& out = config->pParsedAppImageStreamInfo;

  MSize maxStreamSize = MSize(0, 0);
  for (auto const& s : config->vImageStreams) {
    bool isOut = false;
    auto const& pStreamInfo = s.second;
    if (CC_LIKELY(pStreamInfo != nullptr)) {
      switch (pStreamInfo->getImgFormat()) {
        case NSCam::eImgFmt_BLOB:  // AS-IS: should be removed in the future
        case NSCam::eImgFmt_JPEG:  // TO-BE: Jpeg Capture
          isOut = true;
          out->pAppImage_Jpeg = pStreamInfo;
          break;
          //
        case NSCam::eImgFmt_YV12:
        case NSCam::eImgFmt_NV12:
        case NSCam::eImgFmt_NV21:
        case NSCam::eImgFmt_YUY2:
        case NSCam::eImgFmt_Y8:
        case NSCam::eImgFmt_Y16:
          if (pStreamInfo->getStreamType() == eSTREAMTYPE_IMAGE_OUT) {
            isOut = true;
            out->vAppImage_Output_Proc.emplace(s.first, pStreamInfo);
            if (!out->hasVideoConsumer && (pStreamInfo->getUsageForConsumer() &
                                           GRALLOC_USAGE_HW_VIDEO_ENCODER)) {
              out->hasVideoConsumer = true;
              out->videoImageSize = pStreamInfo->getImgSize();
              out->hasVideo4K =
                  (out->videoImageSize.w * out->videoImageSize.h > 8000000)
                      ? true
                      : false;
            }
          } else if (pStreamInfo->getStreamType() == eSTREAMTYPE_IMAGE_IN) {
            out->pAppImage_Input_Yuv = pStreamInfo;
          }
          break;
          //
        case NSCam::eImgFmt_CAMERA_OPAQUE:
          if (pStreamInfo->getStreamType() == eSTREAMTYPE_IMAGE_OUT) {
            isOut = true;
            out->pAppImage_Output_Priv = pStreamInfo;
          } else if (pStreamInfo->getStreamType() == eSTREAMTYPE_IMAGE_IN) {
            out->pAppImage_Input_Priv = pStreamInfo;
          } else if (pStreamInfo->getStreamType() == eSTREAMTYPE_IMAGE_INOUT) {
            out->pAppImage_Output_Priv = pStreamInfo;
            out->pAppImage_Input_Priv = pStreamInfo;
          }
          break;
          //
        default:
          CAM_LOGE("[%s] Unsupported format:0x%x", __FUNCTION__,
                   pStreamInfo->getImgFormat());
          break;
      }
    }

    // if (isOut) //[TODO] why only for output?
    {
      if (maxStreamSize.size() <= pStreamInfo->getImgSize().size()) {
        maxStreamSize = pStreamInfo->getImgSize();
      }
    }
  }

  out->maxImageSize = maxStreamSize;

  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
static auto convertToUserConfiguration(
    const PipelineStaticInfo& rPipelineStaticInfo,
    const UserConfigurationParams& rUserConfigurationParams)
    -> std::shared_ptr<PipelineUserConfiguration> {
  auto pParsedAppConfiguration = std::make_shared<ParsedAppConfiguration>();
  if (CC_UNLIKELY(pParsedAppConfiguration == nullptr)) {
    CAM_LOGE("[%s] Fail on make_shared<ParsedAppConfiguration>", __FUNCTION__);
    return nullptr;
  }

  auto pParsedAppImageStreamInfo = std::make_shared<ParsedAppImageStreamInfo>();
  if (CC_UNLIKELY(pParsedAppImageStreamInfo == nullptr)) {
    CAM_LOGE("[%s] Fail on make_shared<ParsedAppImageStreamInfo>",
             __FUNCTION__);
    return nullptr;
  }
  auto out = std::make_shared<PipelineUserConfiguration>();
  if (CC_UNLIKELY(out == nullptr)) {
    CAM_LOGE("[%s] Fail on make_shared<PipelineUserConfiguration>",
             __FUNCTION__);
    return nullptr;
  }

  pParsedAppConfiguration->operationMode =
      rUserConfigurationParams.operationMode;
  pParsedAppConfiguration->sessionParams =
      rUserConfigurationParams.sessionParams;
  pParsedAppConfiguration->isConstrainedHighSpeedMode =
      (pParsedAppConfiguration->operationMode ==
       1 /*CONSTRAINED_HIGH_SPEED_MODE*/);

  out->pParsedAppConfiguration = pParsedAppConfiguration;
  out->pParsedAppImageStreamInfo = pParsedAppImageStreamInfo;
  out->vImageStreams = rUserConfigurationParams.ImageStreams;
  out->vMetaStreams = rUserConfigurationParams.MetaStreams;
  out->vMinFrameDuration = rUserConfigurationParams.MinFrameDuration;
  out->vStallFrameDuration = rUserConfigurationParams.StallFrameDuration;

  parseAppStreamInfo(out);

  return out;
}

/******************************************************************************
 *
 ******************************************************************************/
static auto decidePipelineModelSession(
    IPipelineModelSessionFactory::CreationParams const& creationParams,
    std::shared_ptr<PipelineUserConfiguration> const& pUserConfiguration,
    std::shared_ptr<IPipelineSettingPolicy> const& pSettingPolicy)
    -> std::shared_ptr<IPipelineModelSession> {
  auto convertTo_CtorParams = [=]() {
    return PipelineModelSessionBase::CtorParams{
        .staticInfo =
            {
                .pPipelineStaticInfo = creationParams.pPipelineStaticInfo,
                .pUserConfiguration = pUserConfiguration,
            },
        .debugInfo = {},
        .pPipelineModelCallback = creationParams.pPipelineModelCallback,
        .pPipelineSettingPolicy = pSettingPolicy,
    };
  };

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  [Session Policy] decide which session
  //  Add special sessions below...
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  auto const operationMode =
      pUserConfiguration->pParsedAppConfiguration->operationMode;
  switch (operationMode) {
      /////////////////////////////////////////////////////////////////////////
      //
      /////////////////////////////////////////////////////////////////////////
    case 0 /* NORMAL_MODE - [FIXME] hard-code */: {
      MY_LOGD("normal flow isVhdrSensor:%d",
              creationParams.pPipelineStaticInfo->isVhdrSensor);
    } break;

    ////////////////////////////////////////////////////////////////////////////
    default: {
      CAM_LOGE("[%s] Unsupported operationMode:0x%x", __FUNCTION__,
               operationMode);
      return nullptr;
    } break;
  }
  MY_LOGD("create default");
  ////////////////////////////////////////////////////////////////////////////
  //  Session: Default
  ////////////////////////////////////////////////////////////////////////////
  return PipelineModelSessionDefault::makeInstance("Default/",
                                                   convertTo_CtorParams());
}

/******************************************************************************
 *
 ******************************************************************************/
auto IPipelineModelSessionFactory::createPipelineModelSession(
    CreationParams const& params __unused)
    -> std::shared_ptr<IPipelineModelSession> {
#undef CHECK_PTR
#define CHECK_PTR(_ptr_, _msg_)                                 \
  if (CC_UNLIKELY(_ptr_ == nullptr)) {                          \
    CAM_LOGE("[%s] nullptr pointer - %s", __FUNCTION__, _msg_); \
    return nullptr;                                             \
  }

  //  (1) validate input parameters.
  CHECK_PTR(params.pPipelineStaticInfo, "pPipelineStaticInfo");
  CHECK_PTR(params.pUserConfigurationParams, "pUserConfigurationParams");
  CHECK_PTR(params.pPipelineModelCallback, "pPipelineModelCallback");

  //  (2) convert to UserConfiguration, return PipelineUserConfiguration
  auto pUserConfiguration = convertToUserConfiguration(
      *params.pPipelineStaticInfo, *params.pUserConfigurationParams);
  CHECK_PTR(pUserConfiguration, "convertToUserConfiguration");

  //  (3) pipeline policy
  auto pSettingPolicy =
      IPipelineSettingPolicyFactory::createPipelineSettingPolicy(
          IPipelineSettingPolicyFactory::CreationParams{
              .pPipelineStaticInfo = params.pPipelineStaticInfo,
              .pPipelineUserConfiguration = pUserConfiguration,
          });
  CHECK_PTR(pSettingPolicy, "Fail on createPipelineSettingPolicy");

  //  (4) pipeline session
  auto pSession =
      decidePipelineModelSession(params, pUserConfiguration, pSettingPolicy);
  CHECK_PTR(pSession, "Fail on decidePipelineModelSession");

  return pSession;
}
