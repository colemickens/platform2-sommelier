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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_STREAMID_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_STREAMID_H_
//
#include <mtkcam/pipeline/stream/StreamId.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 * HW Pipeline Stream ID
 ******************************************************************************/
enum : int64_t {
  //==========================================================================
  eSTREAMID_META_APP_CONTROL = eSTREAMID_END_OF_FWK,
  //==========================================================================
  eSTREAMID_BEGIN_OF_PIPE = (eSTREAMID_BEGIN_OF_INTERNAL),
  //==========================================================================

  eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_00,

  eSTREAMID_IMAGE_PIPE_RAW_RESIZER_00,

  eSTREAMID_IMAGE_PIPE_RAW_LCSO_00,

  eSTREAMID_IMAGE_PIPE_RAW_RSSO_00,

  eSTREAMID_IMAGE_PIPE_RAW_OPAQUE_01,

  eSTREAMID_IMAGE_PIPE_RAW_RESIZER_01,

  eSTREAMID_IMAGE_PIPE_RAW_LCSO_01,

  eSTREAMID_IMAGE_PIPE_RAW_RSSO_01,

  eSTREAMID_IMAGE_PIPE_YUV_JPEG_00,

  eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL_00,

  eSTREAMID_IMAGE_FD,

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  eSTREAMID_META_PIPE_CONTROL_00_SENSOR,

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  eSTREAMID_META_PIPE_CONTROL,
  eSTREAMID_META_PIPE_CONTROL_MAIN2,
  eSTREAMID_META_PIPE_DYNAMIC_01,
  eSTREAMID_META_PIPE_DYNAMIC_01_MAIN2,
  eSTREAMID_META_PIPE_DYNAMIC_02,
  eSTREAMID_META_PIPE_DYNAMIC_02_CAP,
  eSTREAMID_META_PIPE_DYNAMIC_PDE,

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // metadata for event callback
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  eSTREAMID_META_APP_DYNAMIC_CALLBACK,

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  eSTREAMID_META_APP_DYNAMIC_01,
  eSTREAMID_META_APP_DYNAMIC_01_MAIN2,
  eSTREAMID_META_APP_DYNAMIC_02,
  eSTREAMID_META_APP_DYNAMIC_02_CAP,
  eSTREAMID_META_APP_DYNAMIC_FD,
  eSTREAMID_META_APP_DYNAMIC_JPEG,

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  eSTREAMID_IMAGE_HDR,
  eSTREAMID_META_PIPE_DYNAMIC_HDR,
  eSTREAMID_META_APP_DYNAMIC_HDR,

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  eSTREAMID_IMAGE_NR3D,

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // VSDoF
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  eSTREAMID_IMAGE_PIPE_JPS_MAIN,
  eSTREAMID_IMAGE_PIPE_JPS_SUB,
  eSTREAMID_META_PIPE_DYNAMIC_03,
  eSTREAMID_META_PIPE_DYNAMIC_DEPTH,
  eSTREAMID_IMAGE_PIPE_YUV_JPS,
  eSTREAMID_IMAGE_PIPE_YUV_THUMBNAIL_JPS,

  //==========================================================================
  eSTREAMID_END_OF_PIPE = eSTREAMID_END_OF_INTERNAL,
  //==========================================================================
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_HWNODE_STREAMID_H_
