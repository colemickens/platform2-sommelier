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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_IMAGEDESC_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_IMAGEDESC_H_

namespace NSCam {

/**
 * ID of image description which attached in IImageBuffer.
 * All IDs must be registered here globally to avoid potential conflict.
 *
 * @see IImageBuffer::setImgDesc(), IImageBuffer::getImgDesc()
 */
enum ImageDescId {
  /** RAW type of the buffer: pure RAW or processed RAW, etc
   * The value will by filled by P1Node and consumed by P2 driver eventually.
   * @see ImageDescRawType
   */
  eIMAGE_DESC_ID_RAW_TYPE,

  eIMAGE_DESC_ID_MAX
};

const int kImageDescIdMax = eIMAGE_DESC_ID_MAX;

/**
 * Value declaration of eIMAGE_DESC_ID_RAW_TYPE
 */
enum ImageDescRawType {
  /// Not RAW / invalid type
  eIMAGE_DESC_RAW_TYPE_INVALID,
  /// Pure RAW
  eIMAGE_DESC_RAW_TYPE_PURE,
  /// Processed RAW
  eIMAGE_DESC_RAW_TYPE_PROCESSED
};

}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_IMAGEDESC_H_
