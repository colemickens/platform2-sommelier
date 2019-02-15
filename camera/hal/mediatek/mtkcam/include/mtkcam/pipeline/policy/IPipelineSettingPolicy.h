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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IPIPELINESETTINGPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IPIPELINESETTINGPOLICY_H_
//
#include "./types.h"
//
#include <mtkcam/pipeline/pipeline/PipelineContext.h>
//
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace pipelinesetting {

/**
 * Used on the output of evaluateConfiguration().
 *
 */
struct ConfigurationOutputParams {
  /**
   * @param pStreamingFeatureSetting: The streaming feature settings.
   *
   * Callers must ensure it's not a nullptr before this call.
   * Callee will fill its content on this call.
   */
  StreamingFeatureSetting* pStreamingFeatureSetting = nullptr;

  /**
   * @param pStreamingFeatureSetting: The capture feature settings.
   *
   * Callers must ensure it's not a nullptr before this call.
   * Callee will fill its content on this call.
   */
  CaptureFeatureSetting* pCaptureFeatureSetting = nullptr;

  /**
   * It indicates which pipeline nodes are needed.
   *
   * Callers must ensure it's not a nullptr before this call.
   * Callee will fill its content on this call.
   */
  PipelineNodesNeed* pPipelineNodesNeed = nullptr;

  /**
   * The sensor settings.
   *
   * Callers must ensure it's not a nullptr before this call.
   * Callee will fill its content on this call.
   * During reconfiguration, due to sensorSetting changing, callers must fill in
   * the updated settings, and enable related flags in ConfigurationInputParams,
   * so that they won't be modified in SensorSettingPolicy.
   */
  std::vector<SensorSetting>* pSensorSetting = nullptr;

  /**
   * P1 hardware settings.
   *
   * Callers must ensure it's not a nullptr before this call.
   * Callee will fill its content on this call.
   */
  std::vector<P1HwSetting>* pP1HwSetting = nullptr;

  /**
   * P1 DMA need.
   *
   * The value shows which dma are needed.
   * For example,
   *      ((*pvP1DmaNeed)[0] & P1_IMGO) != 0 indicates that IMGO is needed for
   * sensorId[0].
   *      ((*pvP1DmaNeed)[1] & P1_RRZO) != 0 indicates that RRZO is needed for
   * sensorId[1].
   */
  std::vector<uint32_t>* pP1DmaNeed = nullptr;

  /**
   * P1-specific stream info configuration.
   *
   * Callers must ensure it's not a nullptr before this call.
   * Callee will fill its content on this call.
   */
  std::vector<ParsedStreamInfo_P1>* pParsedStreamInfo_P1 = nullptr;

  /**
   * Non-P1-specific stream info configuration.
   *
   * Callers must ensure it's not a nullptr before this call.
   * Callee will fill its content on this call.
   */
  ParsedStreamInfo_NonP1* pParsedStreamInfo_NonP1 = nullptr;

  bool* pIsZSLMode = nullptr;
};

/**
 * Used on the input of evaluateConfiguration().
 *
 */
struct ConfigurationInputParams {
  /**
   * Flags to tell which params need to be modified during reconfiguration.
   * Callers must enable the flag for each param needs to be modified.
   */
  bool bypassSensorSetting = false;
};

/**
 * Used in the following structures:
 *      RequestOutputParams
 */
struct RequestResultParams {
  using NodeSet = NSCam::v3::NSPipelineContext::NodeSet;
  using NodeEdgeSet = NSCam::v3::NSPipelineContext::NodeEdgeSet;
  using IOMapSet = NSCam::v3::NSPipelineContext::IOMapSet;

  /**
   *  Pipeline nodes need.
   *  true indicates its corresponding pipeline node is needed.
   */
  PipelineNodesNeed nodesNeed;

  /**
   *  Node set of a pipeline used
   */
  std::vector<NodeId_T> nodeSet;

  /**
   * The root nodes of a pipeline.
   */
  NodeSet roots;

  /**
   * The edges to connect pipeline nodes.
   */
  NodeEdgeSet edges;

  /**
   * IOMapSet for all pipeline nodes.
   */
  std::unordered_map<NodeId_T, IOMapSet> nodeIOMapImage;
  std::unordered_map<NodeId_T, IOMapSet> nodeIOMapMeta;

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
   * Updated image stream info.
   *
   * For example, they could be Jpeg_YUV and Thumbnail_YUV for capture with
   * rotation.
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IImageStreamInfo>>
      vUpdatedImageStreamInfo;

  /**
   * Additional metadata
   *
   * The design should avoid to override the app metadata control as possible as
   * we can.
   */
  std::shared_ptr<IMetadata> additionalApp;
  std::vector<std::shared_ptr<IMetadata>> additionalHal;
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

  /**
   * Reconfig Category
   * 0:No reconfig, 1:Stream reconfig, 2:Capture reconfig
   */
  ReCfgCtg reconfigCategory = ReCfgCtg::NO;

  /**
   *  The result sensor setting
   */
  std::vector<uint32_t> sensorModes;
  // boost BWC
  uint32_t boostScenario = -1;
  uint32_t featureFlag = 0;
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
   * Request App image stream info, sent at the request stage.
   *
   */
  ParsedAppImageStreamInfo const* pRequest_AppImageStreamInfo = nullptr;

  /**
   * Request App metadata control, sent at the request stage.
   *
   * pRequest_ParsedAppMetaControl is a partial parsed result from
   * pRequest_AppControl, just for the purpose of a quick reference.
   */
  IMetadata const* pRequest_AppControl = nullptr;
  ParsedMetaControl const* pRequest_ParsedAppMetaControl = nullptr;

  /*************************************************************************
   * Configuration info.
   *
   * The final configuration information of the pipeline decided at the
   * configuration stage are as below.
   *
   **************************************************************************/

  /**
   * Configured pipeline nodes, built up at the configuration stage.
   *
   * It indicates which pipeline nodes are configured at the configuration
   * stage.
   */
  PipelineNodesNeed const* pConfiguration_PipelineNodesNeed = nullptr;

  /**
   * Configuration stream info, built up at the configuration stage.
   *
   * Parsed Non-P1 stream info.
   */
  ParsedStreamInfo_NonP1 const* pConfiguration_StreamInfo_NonP1 = nullptr;

  /**
   * Configuration stream info, built up at the configuration stage.
   *
   * Parsed P1 stream info.
   */
  std::vector<ParsedStreamInfo_P1> const* pConfiguration_StreamInfo_P1 =
      nullptr;

  /**************************************************************************
   * Current Setting
   *
   **************************************************************************/

  /**
   * The current sensor setting
   *
   * pSensorMode and pSensorSize are the pointers to two arraies of sensor modes
   * and sensor sizes, respectively.
   * The array size is the same to the size of sensor id (i.e.
   * PipelineStaticInfo::sensorId).
   */
  std::vector<uint32_t> const* pSensorMode = nullptr;
  std::vector<MSize> const* pSensorSize = nullptr;
  //
  bool isZSLMode = false;
};

/**
 *
 */
class IPipelineSettingPolicy {
 public:
  virtual ~IPipelineSettingPolicy() = default;

  /**
   * The policy is in charge of deciding the maximum buffer number of each App
   * image stream which must be decided at the configuration stage.
   *
   * @param[in/out] pInOut
   *  Before this call, callers must promise each App image stream info
   * instance. On this call, each App image stream info's 'setMaxBufNum()' must
   * be called to set up its maximum buffer number.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual auto decideConfiguredAppImageStreamMaxBufNum(
      ParsedAppImageStreamInfo* pInOut,
      StreamingFeatureSetting const* pStreamingFeatureSetting,
      CaptureFeatureSetting const* pCaptureFeatureSetting) -> int = 0;

  /**
   * The policy is in charge of deciding the configuration settings at the
   * configuration stage.
   *
   * @param[out] out: output parameters.
   *
   * @param[in] in: input parameters.
   *
   * @return
   *      0 indicates success; otherwise failure.
   */
  virtual auto evaluateConfiguration(ConfigurationOutputParams* out,
                                     ConfigurationInputParams const& in)
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
   *      0 indicates success; otherwise failure.
   */
  virtual auto evaluateRequest(RequestOutputParams* out,
                               RequestInputParams const& in) -> int = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
class VISIBILITY_PUBLIC IPipelineSettingPolicyFactory {
 public:  ////
  /**
   * A structure for creation parameters.
   */
  struct CreationParams {
    std::shared_ptr<PipelineStaticInfo const> pPipelineStaticInfo;
    std::shared_ptr<PipelineUserConfiguration const> pPipelineUserConfiguration;
  };

  static auto createPipelineSettingPolicy(CreationParams const& params)
      -> std::shared_ptr<IPipelineSettingPolicy>;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace pipelinesetting
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IPIPELINESETTINGPOLICY_H_
