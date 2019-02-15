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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_UTILS_BUFALLOCUTIL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_UTILS_BUFALLOCUTIL_H_

#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <memory>

/**************************************************************************
 *     E N U M / S T R U C T / T Y P E D E F    D E C L A R A T I O N     *
 **************************************************************************/

/*************************************************************************
 *
 **************************************************************************/
namespace NSCam {

/***************************************************************************
 *
 ****************************************************************************/
class BufAllocUtil {
 public:
  // Use this function to allocate IImageBuffer memory.
  // [para] name : You can use LOG_TAG as name.
  // [para] fmt  : ImageFormat, such as eImgFmt_JPEG.
  // [para] w    : Image width
  // [para] h    : Image Height
  // [para] isContinous : Allocate continuous memory or not. JPEG fomat is
  // always continous.
  //                      Default value : MTRUE
  std::shared_ptr<IImageBuffer> allocMem(char const* name,
                                         MUINT32 fmt,
                                         MUINT32 w,
                                         MUINT32 h,
                                         MBOOL isContinous = MTRUE);

  // Use this function to de-allocate IImageBuffer memory.
  // [para] name : You can use LOG_TAG as name. Must be the same as calling
  // allocMem().
  void deAllocMem(char const* name, std::shared_ptr<IImageBuffer> pBuf);
};
};      // namespace NSCam
#endif  //  CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_UTILS_BUFALLOCUTIL_H_
