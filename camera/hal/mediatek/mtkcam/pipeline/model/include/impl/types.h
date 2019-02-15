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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_INCLUDE_IMPL_TYPES_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_INCLUDE_IMPL_TYPES_H_

#include <mtkcam/pipeline/policy/types.h>
#include <mtkcam/pipeline/stream/IStreamBuffer.h>
#include <memory>
#include <string>
#include <unordered_map>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/******************************************************************************
 *
 *  Pipeline Static Information.
 *
 ******************************************************************************/
using PipelineStaticInfo = NSCam::v3::pipeline::policy::PipelineStaticInfo;

/******************************************************************************
 *
 *  1st Configuration related definitions.
 *
 ******************************************************************************/

/**
 *  Parsed App configuration
 */
using ParsedAppConfiguration =
    NSCam::v3::pipeline::policy::ParsedAppConfiguration;

/**
 *  App image stream info configuration
 */
using ParsedAppImageStreamInfo =
    NSCam::v3::pipeline::policy::ParsedAppImageStreamInfo;

/**
 *  Pipeline user configuration
 */
using PipelineUserConfiguration =
    NSCam::v3::pipeline::policy::PipelineUserConfiguration;

/******************************************************************************
 *
 *  2nd Configuration related definitions.
 *
 ******************************************************************************/

/**
 *  (Pass1-specific) stream info configuration
 */
using ParsedStreamInfo_P1 = NSCam::v3::pipeline::policy::ParsedStreamInfo_P1;

/**
 *  (Non Pass1-specific) stream info configuration
 */
using ParsedStreamInfo_NonP1 =
    NSCam::v3::pipeline::policy::ParsedStreamInfo_NonP1;

/**
 *  Pipeline nodes need.
 *  true indicates its corresponding pipeline node is needed.
 */
using PipelineNodesNeed = NSCam::v3::pipeline::policy::PipelineNodesNeed;

/**
 *  Sensor Setting
 */
using SensorSetting = NSCam::v3::pipeline::policy::SensorSetting;

/**
 *  Pass1-specific HW settings
 */
using P1HwSetting = NSCam::v3::pipeline::policy::P1HwSetting;

/**
 * Streaming feature settings
 */
using StreamingFeatureSetting =
    NSCam::v3::pipeline::policy::StreamingFeatureSetting;

/**
 * Capture feature settings
 */
using CaptureFeatureSetting =
    NSCam::v3::pipeline::policy::CaptureFeatureSetting;

/******************************************************************************
 *
 *  Request related definitions.
 *
 ******************************************************************************/

/**
 *  Parsed App image stream buffers
 */
struct ParsedAppImageStreamBuffers {
  /**************************************************************************
   *  App image stream buffer set
   **************************************************************************/

  /**
   * Output streams for any processed (but not-stalling) formats
   *
   * Reference:
   * https://developer.android.com/reference/android/hardware/camera2/CameraCharacteristics.html#REQUEST_MAX_NUM_OUTPUT_PROC
   */
  std::unordered_map<StreamId_T, std::shared_ptr<IImageStreamBuffer>>
      vAppImage_Output_Proc;

  /**
   * Input stream for yuv reprocessing
   */
  std::shared_ptr<IImageStreamBuffer> pAppImage_Input_Yuv;

  /**
   * Output stream for private reprocessing
   */
  std::shared_ptr<IImageStreamBuffer> pAppImage_Output_Priv;

  /**
   * Input stream for private reprocessing
   */
  std::shared_ptr<IImageStreamBuffer> pAppImage_Input_Priv;
  /**
   * Output stream for JPEG capture.
   */
  std::shared_ptr<IImageStreamBuffer> pAppImage_Jpeg;

  inline std::string toString() const {
    std::string os;
    for (auto const& v : vAppImage_Output_Proc) {
      os += "\n    ";
      os += v.second->toString();
    }
    if (auto p = pAppImage_Input_Yuv) {
      os += "\n    ";
      os += p->toString();
    }
    if (auto p = pAppImage_Output_Priv) {
      os += "\n    ";
      os += p->toString();
    }
    if (auto p = pAppImage_Input_Priv) {
      os += "\n    ";
      os += p->toString();
    }
    if (auto p = pAppImage_Jpeg) {
      os += "\n    ";
      os += p->toString();
    }
    return os;
  }
};

/**
 *  Parsed App request
 */
struct ParsedAppRequest {
  using ParsedMetaControl = NSCam::v3::pipeline::policy::ParsedMetaControl;

  /**
   * Request number.
   */
  uint32_t requestNo = 0;

  /**
   * App metadata control (stream buffer), sent at the request stage.
   *
   * pParsedAppMetaControl is a partial parsed result from pAppMetaControl, just
   * for the purpose of a quick reference.
   */
  std::shared_ptr<IMetaStreamBuffer> pAppMetaControlStreamBuffer = nullptr;
  std::shared_ptr<ParsedMetaControl> pParsedAppMetaControl = nullptr;

  /**
   * App image stream buffers, sent at the request stage.
   */
  std::shared_ptr<ParsedAppImageStreamBuffers> pParsedAppImageStreamBuffers;

  /**
   * App image stream info, sent at the request stage.
   */
  std::shared_ptr<ParsedAppImageStreamInfo> pParsedAppImageStreamInfo;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_INCLUDE_IMPL_TYPES_H_
