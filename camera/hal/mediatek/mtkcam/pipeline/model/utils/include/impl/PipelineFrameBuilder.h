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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_PIPELINEFRAMEBUILDER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_PIPELINEFRAMEBUILDER_H_

#include <mtkcam/pipeline/pipeline/PipelineContext.h>

#include <impl/types.h>
#include <memory>
#include <unordered_map>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/**
 * Unsed on the call buildPipelineFrame().
 */
struct BuildPipelineFrameInputParams {
  using HalImageStreamBuffer = NSCam::v3::Utils::HalImageStreamBuffer;
  using HalMetaStreamBuffer = NSCam::v3::Utils::HalMetaStreamBuffer;
  using IOMapSet = NSCam::v3::NSPipelineContext::IOMapSet;

  /**
   * Request no.
   */
  uint32_t const requestNo = 0;

  /**
   * Reprocess frame or not.
   */
  MBOOL const bReprocessFrame = MFALSE;

  /**
   * App image stream buffers.
   */
  ParsedAppImageStreamBuffers const* pAppImageStreamBuffers = nullptr;

  /**
   * App meta stream buffers.
   */
  std::vector<std::shared_ptr<IMetaStreamBuffer>> const* pAppMetaStreamBuffers =
      nullptr;

  /**
   * Hal image stream buffers.
   */
  std::vector<std::shared_ptr<HalImageStreamBuffer>> const*
      pHalImageStreamBuffers = nullptr;

  /**
   * Hal meta stream buffers.
   */
  std::vector<std::shared_ptr<HalMetaStreamBuffer>> const*
      pHalMetaStreamBuffers = nullptr;

  /**
   * Updated image stream info.
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IImageStreamInfo>> const*
      pvUpdatedImageStreamInfo = nullptr;

  /**
   * IOMapSet for all pipeline nodes.
   */
  std::vector<NodeId_T> const* pnodeSet = nullptr;
  std::unordered_map<NodeId_T, IOMapSet> const* pnodeIOMapImage = nullptr;
  std::unordered_map<NodeId_T, IOMapSet> const* pnodeIOMapMeta = nullptr;

  /**
   * The root nodes of a pipeline.
   */
  NSPipelineContext::NodeSet const* pRootNodes = nullptr;

  /**
   * The edges to connect pipeline nodes.
   */
  NSPipelineContext::NodeEdgeSet const* pEdges = nullptr;

  /**
   * RequestBuilder's callback.
   */
  std::weak_ptr<NSPipelineContext::RequestBuilder::AppCallbackT> pCallback;

  /**
   * Pipeline context
   */
  std::shared_ptr<NSPipelineContext::PipelineContext> const pPipelineContext =
      nullptr;
};

/**
 * Generate a new pipeline frame.
 *
 * @param[out] out: a new-created pipeline frame.
 *
 * @param[in] in: input parameters.
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
auto VISIBILITY_PUBLIC
buildPipelineFrame(std::shared_ptr<IPipelineFrame>* out,
                   BuildPipelineFrameInputParams const& in) -> int;

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_PIPELINEFRAMEBUILDER_H_
