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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_IMAGEBUFFERINFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_IMAGEBUFFERINFO_H_

#include <vector>
#include "ImageFormat.h"
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *  BufPlane
 ******************************************************************************/

struct BufPlane {
  /**
   * The size for this color plane, in bytes.
   */

  size_t sizeInBytes = 0;

  /**
   * The row stride for this color plane, in bytes.
   *
   * This is the distance between the start of two consecutive rows of
   * pixels in the image. The row stride is always greater than 0.
   */

  size_t rowStrideInBytes = 0;
};

/******************************************************************************
 *  ImageBufferInfo
 ******************************************************************************/
struct ImageBufferInfo {
  /**********************************************************************
   * Buffer information
   */

  /**
   * The offset, in bytes, of each buffer.
   *
   * bufOffset.size() indicates the buffer count, and
   * bufOffset[i] represents the offset of the i-th buffer.
   */
  std::vector<size_t> bufOffset;

  /**
   * The planes of per-buffer.
   * For example, 3 planes for YV12 and 2 planes for NV21.
   */
  std::vector<BufPlane> bufPlanes;

  /**********************************************************************
   * Image information
   */

  /**
   * The image format (eImgFmt_XXX).
   */
  int imgForamt = eImgFmt_UNKNOWN;

  /**
   * The image size, in pixels.
   */
  MSize imgSize;
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_IMAGEBUFFERINFO_H_
