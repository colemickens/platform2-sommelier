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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_CUSTOMER_CUSTOMER_FEATURE_TYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_CUSTOMER_CUSTOMER_FEATURE_TYPE_H_

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSPipelinePlugin {

enum eFeatureIndexCustomer {
  // ThirdParty (bit 32-63)
  TP_FEATURE_HDR = 1ULL << 32,
  TP_FEATURE_MFNR = 1ULL << 33,
  TP_FEATURE_EIS = 1ULL << 34,
  TP_FEATURE_FB = 1ULL << 35,
  TP_FEATURE_FILTER = 1ULL << 36,
  TP_FEATURE_DEPTH = 1ULL << 37,
  TP_FEATURE_BOKEH = 1ULL << 38,
  TP_FEATURE_VSDOF = (TP_FEATURE_DEPTH | TP_FEATURE_BOKEH),
  TP_FEATURE_FUSION = 1ULL << 39,
  TP_FEATURE_HDR_DC = 1ULL << 40,  // used by DualCam
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSPipelinePlugin
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_3RDPARTY_CUSTOMER_CUSTOMER_FEATURE_TYPE_H_
