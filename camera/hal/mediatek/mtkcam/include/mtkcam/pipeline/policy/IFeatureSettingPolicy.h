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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IFEATURESETTINGPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IFEATURESETTINGPOLICY_H_
//
#include <map>
#include <memory>
#include <vector>
//
#include <mtkcam/def/common.h>
#include <mtkcam/pipeline/policy/types.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace featuresetting {

/**
 * Used in the following structures:
 *      RequestOutputParams
 */
struct RequestResultParams {
  /**
   * P1 DMA need, sent at the request stage.
   *
   * The value shows which dma are needed.
   * For example,
   *      (needP1Dma[0] & P1_IMGO) != 0 indicates that IMGO is needed for
   * sensorId[0]. (needP1Dma[1] & P1_RRZO) != 0 indicates that RRZO is needed
   * for sensorId[1].
   */
  std::vector<uint32_t> needP1Dma;

  /**
   * Additional metadata
   *
   * The design should avoid to override the app metadata control as possible as
   * we can.
   */
  std::shared_ptr<IMetadata> additionalApp;
  // for multicam case, it will contain more than one hal metadata.
  std::vector<std::shared_ptr<IMetadata>> vAdditionalHal;
};

/**
 * Used on the output of evaluateRequest().
 *
 * Request-stage policy
 *  $ Need to re-configure or not?
 *  $ New sensor mode settings if changed (e.g. 4cell)
 *  $ The frame sequence: pre-dummy frames, main frame, sub frames, post-dummy
 * frames $ IMGO / RRZO / RSSO settings . Process RAW (e.g. type3 PD sensor) .
 * format: UFO/Unpack RAW/Pack RAW (e.g. HDR) . size (e.g. streaming) $ Frame
 * rate change (via metadata) (e.g. 60fps capture for special sensors) $ ZSL
 * flow or non-ZSL flow $ ZSL selection policy $ ......
 */
struct RequestOutputParams {
  /**
   * If this is true, it means this policy requests to re-configure the
   * pipeline. In this case, all the following output results are evaluated
   * based on the after-reconfiguration setting, not the before-reconfiguration
   * setting.
   */
  bool needReconfiguration = false;

  /**
   * The frame sequence is as below:
   *       pre dummy frame 0
   *       pre dummy frame ...
   *       pre dummy frame X-1
   *            main frame        (should be aligned to the request sent from
   * users) sub frame 0 sub frame ... sub frame Y-1 post dummy frame 0 post
   * dummy frame ... post dummy frame Z-1
   *
   * The policy module is in charge of allocating the memory when needed.
   */
  std::shared_ptr<RequestResultParams> mainFrame;
  std::vector<std::shared_ptr<RequestResultParams>> subFrames;
  std::vector<std::shared_ptr<RequestResultParams>>
      preDummyFrames;  //  need dummy frames
  std::vector<std::shared_ptr<RequestResultParams>>
      postDummyFrames;  //  need dummy frames

  /**
   * ZSL still capture flow is needed if true; otherwise not needed.
   */
  bool needZslFlow = false;
  ZslPolicyParams zslPolicyParams;

  // boost BWC
  uint32_t boostScenario = -1;
  uint32_t featureFlag = 0;

  /**
   * Reconfig Category
   * 0:No reconfig, 1:Stream reconfig, 2:Capture reconfig
   */
  ReCfgCtg reconfigCategory = ReCfgCtg::NO;

  /**
   *  The result sensor setting
   */
  std::vector<uint32_t> sensorModes;

  /**
   * [TODO]
   *
   *  $ New sensor mode settings if changed (e.g. 4cell)
   *  $ IMGO / RRZO / RSSO settings
   *    . Process RAW (e.g. type3 PD sensor)
   *    . format: UFO/Unpack RAW/Pack RAW (e.g. HDR)
   *    . size (e.g. streaming)
   *  $ ZSL flow or non-ZSL flow
   *  $ ZSL selection policy
   *  $ ......
   */
};

/**
 * Used on the input of evaluateRequest().
 */
struct RequestInputParams {
  /**************************************************************************
   * Request parameters
   *
   * The parameters related to this capture request is shown as below.
   *
   **************************************************************************/

  /**
   * Request number, sent at the request stage.
   *
   */
  uint32_t requestNo = 0;

  /**
   * Request App metadata control, sent at the request stage.
   *
   * pRequest_ParsedAppMetaControl is a partial parsed result from
   * pRequest_AppControl, just for the purpose of a quick reference.
   */
  IMetadata const* pRequest_AppControl = nullptr;
  ParsedMetaControl const* pRequest_ParsedAppMetaControl = nullptr;

  ParsedAppImageStreamInfo const* pRequest_AppImageStreamInfo = nullptr;

  /**
   * configure stage data
   */
  std::vector<ParsedStreamInfo_P1> const* pConfiguration_StreamInfo_P1 =
      nullptr;
  bool Configuration_HasRecording = false;

  /**
   * request hint for p2 feature pipeline nodes. (from P2NodeDecisionPolicy)
   *
   * true indicates its corresponding request stream out from
   * dedicated feature pipeline node during the request stage.
   */
  bool needP2CaptureNode = false;
  MSize maxP2CaptureSize;
  bool needP2StreamNode = false;
  MSize maxP2StreamSize;

  /**************************************************************************
   * Current Setting
   *
   **************************************************************************/
  /**
   *  The current sensor setting
   */
  std::vector<uint32_t> sensorModes;
};

struct ConfigurationInputParams {
  IMetadata const* pSessionParams = nullptr;
  /**
   *  ZSL buffer pool is existed during current configuration.
   */
  bool isZslMode = false;
};

/**
 * Used on the output of evaluateConfiguration().
 *
 * At the configuration stage, capture feature and streaming feature policy will
 * output their both requirements.
 */
struct ConfigurationOutputParams {
  StreamingFeatureSetting StreamingParams;
  CaptureFeatureSetting CaptureParams;
};

/**
 *
 */
class IFeatureSettingPolicy {
 public:
  virtual ~IFeatureSettingPolicy() = default;

  /**
   * The policy is in charge of reporting its requirement at the configuration
   * stage.
   *
   * @param[in] in:
   *  Callers must promise its content. The callee is not allowed to modify it.
   *
   * @param[out] out:
   *  On this call, the callee must allocate and set up its content.
   *
   * @return
   *      true indicates success; otherwise failure.
   */
  virtual auto evaluateConfiguration(ConfigurationOutputParams* out,
                                     ConfigurationInputParams const* in)
      -> int = 0;

  /**
   * The policy is in charge of reporting its requirement at the request stage.
   *
   * @param[out] out:
   *  Callers must ensure it's a non-nullptr.
   *
   * @param[in] in:
   *  Callers must promise its content. The callee is not allowed to modify it.
   *
   * @return
   *      true indicates success; otherwise failure.
   */
  virtual auto evaluateRequest(RequestOutputParams* out,
                               RequestInputParams const* in) -> int = 0;
};

/**
 * A structure for creation parameters.
 */
struct CreationParams {
  // info for all feature
  std::shared_ptr<PipelineStaticInfo const> pPipelineStaticInfo;

  // info for streaming feature
  std::shared_ptr<PipelineUserConfiguration const> pPipelineUserConfiguration;
};

auto createFeatureSettingPolicyInstance(CreationParams const& params)
    -> std::shared_ptr<IFeatureSettingPolicy>;

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace featuresetting
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IFEATURESETTINGPOLICY_H_
