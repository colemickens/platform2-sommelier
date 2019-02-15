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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ITOPOLOGYPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ITOPOLOGYPOLICY_H_
//
#include "./types.h"

#include <functional>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace topology {

/**
 * A structure definition for output parameters
 */
struct RequestOutputParams {
  using NodeSet = NSCam::v3::NSPipelineContext::NodeSet;
  using NodeEdgeSet = NSCam::v3::NSPipelineContext::NodeEdgeSet;

  /**
   *  Pipeline nodes need.
   *  true indicates its corresponding pipeline node is needed.
   */
  PipelineNodesNeed* pNodesNeed = nullptr;
  std::vector<NodeId_T>* pNodeSet = nullptr;

  /**
   * The root nodes of a pipeline.
   */
  NodeSet* pRootNodes = nullptr;

  /**
   * The edges to connect pipeline nodes.
   */
  NodeEdgeSet* pEdges = nullptr;
};

/**
 * A structure definition for input parameters
 */
struct RequestInputParams {
  /**************************************************************************
   * Request parameters
   *
   * The parameters related to this capture request is shown as below.
   *
   **************************************************************************/

  /**
   * true indicates it intents to create "dummy frame", which means this reqeust
   * will enque to pass1 driver without any target result images.
   */
  bool isDummyFrame = false;

  /**
   * The following info usually results from the P2Node decision policy.
   *
   * Output image/meta stream IDs which P2 streaming node is in charge of
   * outputing. Output image/meta stream IDs which P2 capture node is in charge
   * of outputing.
   */
  bool needP2CaptureNode = false;
  bool needP2StreamNode = false;
  std::vector<StreamId_T> const* pvImageStreamId_from_StreamNode = nullptr;
  std::vector<StreamId_T> const* pvMetaStreamId_from_StreamNode = nullptr;
  std::vector<StreamId_T> const* pvImageStreamId_from_CaptureNode = nullptr;
  std::vector<StreamId_T> const* pvMetaStreamId_from_CaptureNode = nullptr;

  /**
   * true indicates it intents to enable the face detection.
   */
  bool isFdEnabled = false;

  /**
   * Request App image stream info, sent at the request stage.
   *
   */
  ParsedAppImageStreamInfo const* pRequest_AppImageStreamInfo = nullptr;

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

  /*************************************************************************
   * Static info.
   *
   **************************************************************************/

  /**
   * Pipeline static information.
   */
  PipelineStaticInfo const* pPipelineStaticInfo = nullptr;
};

};  // namespace topology

////////////////////////////////////////////////////////////////////////////////

/**
 * The function type definition.
 * It is used to decide the pipeline topology.
 *
 * @param[out] out: output parameters
 *
 * @param[in] in: input parameters
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
using FunctionType_TopologyPolicy =
    std::function<int(topology::RequestOutputParams& /*out*/,
                      topology::RequestInputParams const& /*in*/)>;

//==============================================================================

/**
 * Policy instance makers
 *
 */

// default version
FunctionType_TopologyPolicy makePolicy_Topology_Default();

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ITOPOLOGYPOLICY_H_
