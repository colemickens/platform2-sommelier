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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IIOMAPPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IIOMAPPOLICY_H_
//
#include "./types.h"

#include <functional>
#include <unordered_map>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace iomap {

/**
 * A structure definition for output parameters
 */
struct RequestOutputParams {
  using IOMapSet = NSCam::v3::NSPipelineContext::IOMapSet;

  /**
   * IOMapSet for all pipeline nodes.
   */
  std::unordered_map<NodeId_T, IOMapSet>* pNodeIOMapImage = nullptr;
  std::unordered_map<NodeId_T, IOMapSet>* pNodeIOMapMeta = nullptr;
};

/**
 * A structure definition for input parameters
 */
struct RequestInputParams {
  int isMainFrame = true;
  int isDummyFrame = false;
  /**************************************************************************
   * Request parameters
   *
   * The parameters related to this capture request is shown as below.
   *
   **************************************************************************/

  /**
   * Pipeline nodes need, sent at the request stage.
   */
  PipelineNodesNeed const* pRequest_PipelineNodesNeed = nullptr;

  /**
   * Request App image stream info, sent at the request stage.
   *
   */
  ParsedAppImageStreamInfo const* pRequest_AppImageStreamInfo = nullptr;

  /**
   * The thumbnail size is passed to HAL at the request stage.
   */
  IImageStreamInfo const* pRequest_HalImage_Thumbnail_YUV = nullptr;

  /**
   * P1 DMA need, sent at the request stage.
   *
   * The value shows which dma are needed.
   * For example,
   *      (needP1Dma[0] & P1_IMGO) != 0 indicates that IMGO is needed for
   * sensorId[0]. (needP1Dma[1] & P1_RRZO) != 0 indicates that RRZO is needed
   * for sensorId[1].
   */
  std::vector<uint32_t> const* pRequest_NeedP1Dma = nullptr;

  /**
   * Output image/meta stream IDs which P2 streaming node is in charge of
   * outputing.
   */
  std::vector<StreamId_T> const* pvImageStreamId_from_StreamNode = nullptr;
  std::vector<StreamId_T> const* pvMetaStreamId_from_StreamNode = nullptr;

  /**
   * Output image/meta stream IDs which P2 capture node is in charge of
   * outputing.
   */
  std::vector<StreamId_T> const* pvImageStreamId_from_CaptureNode = nullptr;
  std::vector<StreamId_T> const* pvMetaStreamId_from_CaptureNode = nullptr;

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
   * Configuration stream info, built up at the configuration stage.
   *
   * Parsed P1 stream info.
   */
  std::vector<ParsedStreamInfo_P1> const* pConfiguration_StreamInfo_P1 =
      nullptr;
};

};  // namespace iomap

////////////////////////////////////////////////////////////////////////////////

/**
 * The function type definition.
 * It is used to decide the I/O Map of P2Nodes, including P2StreamNode and
 * P2CaptureNode.
 *
 * @param[out] out: input parameters
 *
 * @param[in] in: input parameters
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
using FunctionType_IOMapPolicy_P2Node =
    std::function<int(iomap::RequestOutputParams const& /*out*/,
                      iomap::RequestInputParams const& /*in*/)>;

/**
 * The function type definition.
 * It is used to decide the I/O Map of Non-P2Node.
 *
 * @param[out] out: input parameters
 *
 * @param[in] in: input parameters
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
using FunctionType_IOMapPolicy_NonP2Node =
    std::function<int(iomap::RequestOutputParams const& /*out*/,
                      iomap::RequestInputParams const& /*in*/)>;

//==============================================================================

/**
 * Policy instance makers
 *
 */

// default version
FunctionType_IOMapPolicy_P2Node makePolicy_IOMap_P2Node_Default();

////////////////////////////////////////////////////////////////////////////////

// default version
FunctionType_IOMapPolicy_NonP2Node makePolicy_IOMap_NonP2Node_Default();

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_IIOMAPPOLICY_H_
