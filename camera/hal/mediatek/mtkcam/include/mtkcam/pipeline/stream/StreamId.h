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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_STREAMID_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_STREAMID_H_
//
#include <stdint.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 * HALv1 streamId table
 *      value          external/internal of pipeline        description
 *     0 ~ 2^31-1               internal                   app's streams
 *   2^31 ~ 2^63-1              internal                   hal's streams
 ******************************************************************************/

/******************************************************************************
 * HALv3 streamId table
 *      value          external/internal of pipeline        description
 *        -1                    external                     NO_STREAM
 *     0 ~ 2^31-1               external                    fwk/client id
 *       2^31                   external                 application's control
 *  2^31+1 ~ 2^32-1             external                preserved for future use
 *   2^32 ~ 2^63-1              internal                    hal's streams
 ******************************************************************************/

enum : int64_t {
  eSTREAMID_NO_STREAM = -1L,
  //==========================================================================
  //==========================================================================
  eSTREAMID_BEGIN_OF_EXTERNAL = (0x00000L),
  //==========================================================================
  eSTREAMID_BEGIN_OF_APP = eSTREAMID_BEGIN_OF_EXTERNAL,
  eSTREAMID_BEGIN_OF_FWK = eSTREAMID_BEGIN_OF_EXTERNAL,
  //======================================================================
  // ...
  //======================================================================
  eSTREAMID_END_OF_APP = (0x80000000L),
  eSTREAMID_END_OF_FWK = eSTREAMID_END_OF_APP,
  //=======================================================================
  // ...
  //==========================================================================
  eSTREAMID_END_OF_EXTERNAL = (0xFFFFFFFFL),
  //==========================================================================
  //==========================================================================
  eSTREAMID_BEGIN_OF_INTERNAL = (0x100000000L),
  //==========================================================================

  //==========================================================================
  eSTREAMID_END_OF_INTERNAL = (0x7FFFFFFFFFFFFFFFL),
  //==========================================================================
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_PIPELINE_STREAM_STREAMID_H_
