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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICAPTURESTREAMUPDATERPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICAPTURESTREAMUPDATERPOLICY_H_
//
#include "./types.h"

#include <functional>
#include <memory>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {
namespace capturestreamupdater {

/**
 * A structure definition for output parameters
 */
struct RequestOutputParams {
  /**
   * The Jpeg orientation is passed to HAL at the request stage.
   */
  std::shared_ptr<IImageStreamInfo>* pHalImage_Jpeg_YUV = nullptr;

  /**
   * The thumbnail size is passed to HAL at the request stage.
   */
  std::shared_ptr<IImageStreamInfo>* pHalImage_Thumbnail_YUV = nullptr;
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
  std::shared_ptr<IImageStreamInfo> const* pConfiguration_HalImage_Jpeg_YUV =
      nullptr;
  std::shared_ptr<IImageStreamInfo> const*
      pConfiguration_HalImage_Thumbnail_YUV = nullptr;

  /*************************************************************************
   * Static info.
   *
   **************************************************************************/

  /**
   * true indicates Jpeg capture with rotation is supported.
   */
  bool isJpegRotationSupported = true;

  /**
   * current sensor id
   */
  MUINT32 sensorID;
};

};  // namespace capturestreamupdater

////////////////////////////////////////////////////////////////////////////////

/**
 * The function type definition.
 * It is used to decide whether or not to update the capture streams.
 *
 * @param[out] out: input parameters
 *
 * @param[in] in: input parameters
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
using FunctionType_CaptureStreamUpdaterPolicy =
    std::function<int(capturestreamupdater::RequestOutputParams& /*out*/,
                      capturestreamupdater::RequestInputParams const& /*in*/)>;

//==============================================================================

/**
 * Policy instance makers
 *
 */

// default version
FunctionType_CaptureStreamUpdaterPolicy
makePolicy_CaptureStreamUpdater_Default();

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace policy
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_POLICY_ICAPTURESTREAMUPDATERPOLICY_H_
