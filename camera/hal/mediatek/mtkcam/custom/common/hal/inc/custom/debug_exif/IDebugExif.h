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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_IDEBUGEXIF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_IDEBUGEXIF_H_
#pragma once
//
#include <map>
//
#include "custom/debug_exif/dbg_exif_def.h"
#include "custom/debug_exif/dbg_exif_param.h"
//
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace Custom {

/******************************************************************************
 *  Custom Debug Exif Interface.
 ******************************************************************************/
class IDebugExif {
 public:  ////                    Destructor.
  virtual ~IDebugExif() {}

 public:  ////                    Attributes.
  /**
   * Get the information of buffer layout.
   */
  virtual debug_exif_buffer_info const* getBufInfo(uint32_t keyID) const = 0;

  virtual uint32_t getTagId_MF_TAG_VERSION() const = 0;
  virtual uint32_t getTagId_MF_TAG_IMAGE_HDR() const = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace Custom
};  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_IDEBUGEXIF_H_
