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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_MTK_MTK_FEATURE_TYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_MTK_MTK_FEATURE_TYPE_H_

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSPipelinePlugin {

enum eFeatureIndexMtk {
  NO_FEATURE_NORMAL = 0ULL,
  // MTK (bit 0-31)
  MTK_FEATURE_MFNR = 1ULL << 0,
  MTK_FEATURE_REMOSAIC = 1ULL << 2,
  MTK_FEATURE_ABF = 1ULL << 3,
  MTK_FEATURE_NR = 1ULL << 4,
  MTK_FEATURE_3DNR = 1ULL << 11,
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSPipelinePlugin
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_MTK_MTK_FEATURE_TYPE_H_
