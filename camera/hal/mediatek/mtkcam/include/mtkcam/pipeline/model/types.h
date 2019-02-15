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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_MODEL_TYPES_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_MODEL_TYPES_H_

#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <memory>
#include <string>
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
 * Pipeline User Configuration Parameters, used by IPipelineModel::configure().
 */
struct UserConfigurationParams {
  /**
   * @param[in] The operation mode of pipeline.
   *  The caller must promise its value.
   */
  uint32_t operationMode = 0;

  /**
   * Session wide camera parameters.
   *
   * The session parameters contain the initial values of any request keys that
   * were made available via ANDROID_REQUEST_AVAILABLE_SESSION_KEYS. The Hal
   * implementation can advertise any settings that can potentially introduce
   * unexpected delays when their value changes during active process requests.
   * Typical examples are parameters that trigger time-consuming HW
   * re-configurations or internal camera pipeline updates. The field is
   * optional, clients can choose to ignore it and avoid including any initial
   * settings. If parameters are present, then hal must examine their values and
   * configure the internal camera pipeline accordingly.
   */
  IMetadata sessionParams;

  /**
   * @param[in] App image streams to configure.
   *  The caller must promise the number of buffers and each content.
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IImageStreamInfo>>
      ImageStreams;

  /**
   * @param[in] App meta streams to configure.
   *  The caller must promise the number of buffers and each content.
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IMetaStreamInfo>> MetaStreams;

  /**
   * @param[in] App image streams min frame duration to configure.
   *  The caller must promise its initial value.
   */
  std::unordered_map<StreamId_T, int64_t> MinFrameDuration;

  /**
   * @param[in] App image streams stall frame duration to configure.
   *  The caller must promise its initial value.
   */
  std::unordered_map<StreamId_T, int64_t> StallFrameDuration;

  /**
   * @param[in] physical camera id list
   */
  std::vector<int32_t> vPhysicCameras;
};

/**
 * Pipeline User Request Params, used by IPipelineModel::submitRequest().
 */
struct UserRequestParams {
  /**
   * @param[in] request number.
   *  The caller must promise its content.
   *  The callee can not modify it.
   */
  uint32_t requestNo = 0;

  /**
   * @param[in,out] input App image stream buffers, if any.
   *  The caller must promise the number of buffers and each content.
   *  The callee will update each buffer's users.
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IImageStreamBuffer>>
      IImageBuffers;

  /**
   * @param[in,out] output App image stream buffers.
   *  The caller must promise the number of buffers and each content.
   *  The callee will update each buffer's users.
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IImageStreamBuffer>>
      OImageBuffers;

  /**
   * @param[in,out] input App meta stream buffers.
   *  The caller must promise the number of buffers and each content.
   *  The callee will update each buffer's users.
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IMetaStreamBuffer>>
      IMetaBuffers;
};

/**
 * Used by IPipelineModelCallback::onFrameUpdated().
 */
struct UserOnFrameUpdated {
  /**
   * @param[in] request number.
   */
  uint32_t requestNo = 0;

  /**
   * @param[in] user id.
   *  In fact, it is pipeline node id from the viewpoint of pipeline
   * implementation, but the pipeline users have no such knowledge.
   */
  intptr_t userId = 0;

  /**
   * @param[in] how many output metadata are not finished.
   */
  ssize_t nOutMetaLeft = 0;

  /**
   * @param[in] (partial) output metadata.
   */
  std::vector<std::shared_ptr<IMetaStreamBuffer>> vOutMeta;

  /**
   * @param[in] the timestamp of the start of frame.
   */
  int64_t timestampStartOfFrame = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_MODEL_TYPES_H_
