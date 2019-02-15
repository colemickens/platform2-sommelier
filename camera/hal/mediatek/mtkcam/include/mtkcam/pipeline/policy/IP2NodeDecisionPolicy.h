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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IP2NODEDECISIONPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IP2NODEDECISIONPOLICY_H_
//
#include "./types.h"
//
#include <functional>
#include <memory>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

namespace p2nodedecision {

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

  /**
   * Request App face detection intent, resulted at the request stage.
   *
   * true indicates it intents to enable the face detection.
   */
  bool isFdEnabled = false;
  bool needThumbnail = false;

  /*************************************************************************
   * Configuration info.
   *
   * The final configuration information of the pipeline decided at the
   * configuration stage are as below.
   *
   **************************************************************************/

  /**
   * Configuration stream info, built up at the configuration stage.
   *
   * Parsed Non-P1 stream info.
   */
  ParsedStreamInfo_NonP1 const* pConfiguration_StreamInfo_NonP1 = nullptr;
  /**
   * Parsed main1 stream info.
   */
  ParsedStreamInfo_P1 const* pConfiguration_StreamInfo_P1 = nullptr;

  /**
   * Configuration pipeline nodes, built up at the configuration stage.
   *
   * true indicates its corresponding pipeline node is built-up and enabled
   * at the configuration stage.
   */
  bool hasP2CaptureNode = false;
  bool hasP2StreamNode = false;
};

/**
 * Used on the output of evaluateRequest().
 */
struct RequestOutputParams {
  bool needP2CaptureNode = false;
  MSize maxP2CaptureSize;
  bool needP2StreamNode = false;
  MSize maxP2StreamSize;

  std::vector<StreamId_T> vImageStreamId_from_CaptureNode;
  std::vector<StreamId_T> vImageStreamId_from_StreamNode;

  std::vector<StreamId_T> vMetaStreamId_from_CaptureNode;
  std::vector<StreamId_T> vMetaStreamId_from_StreamNode;
};

};  // namespace p2nodedecision

////////////////////////////////////////////////////////////////////////////////

/**
 * The following nodes belong to P2 node group:
 *      P2CaptureNode
 *      P2StreamingNode
 *
 * The policy is in charge of reporting its requirement at the request stage.
 * Given a request, this policy module is in charge of deciding which P2 nodes
 * are responsible for generating which output image & meta streams.
 *
 *
 * @param[out] out: input parameters
 *
 * @param[in] in: input parameters
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
using FunctionType_P2NodeDecisionPolicy =
    std::function<int(p2nodedecision::RequestOutputParams* /*out*/,
                      p2nodedecision::RequestInputParams const& /*in*/)>;

//==============================================================================

/**
 * Policy instance makers
 *
 */

// default version
FunctionType_P2NodeDecisionPolicy makePolicy_P2NodeDecision_Default();

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IP2NODEDECISIONPOLICY_H_
