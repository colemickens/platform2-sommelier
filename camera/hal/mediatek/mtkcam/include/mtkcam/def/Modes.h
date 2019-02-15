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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_MODES_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_MODES_H_

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/**
 * @enum EAppMode
 * @brief App Mode Enumeration.
 *
 */
enum EAppMode {
  eAppMode_DefaultMode = 0, /*!< Default Mode */
  eAppMode_EngMode,         /*!< Engineer Mode */
  eAppMode_AtvMode,         /*!< ATV Mode */
  eAppMode_StereoMode,      /*!< Stereo Mode */
  eAppMode_VtMode,          /*!< VT Mode */
  eAppMode_PhotoMode,       /*!< Photo Mode */
  eAppMode_VideoMode,       /*!< Video Mode */
  eAppMode_ZsdMode,         /*!< ZSD Mode */
  eAppMode_FactoryMode,     /*!< Factory Mode */
  eAppMode_DualCam,         /*!< Dual Zoom Mode */
};

/**
 * @enum EShotMode
 * @brief Shot Mode Enumeration.
 *
 */
enum EShotMode {
  eShotMode_NormalShot,       /*!< Normal Shot */
  eShotMode_ContinuousShot,   /*!< Continuous Shot Ncc*/
  eShotMode_ContinuousShotCc, /*!< Continuous Shot Cc*/
  eShotMode_BestShot,         /*!< Best Select Shot */
  eShotMode_EvShot,           /*!< Ev-bracketshot Shot */
  eShotMode_SmileShot,        /*!< Smile-detection Shot */
  eShotMode_HdrShot,          /*!< High-dynamic-range Shot */
  eShotMode_AsdShot,          /*!< Auto-scene-detection Shot */
  eShotMode_ZsdShot,          /*!< Zero-shutter-delay Shot */
  eShotMode_FaceBeautyShot,   /*!<  Face-beautifier Shot */
  eShotMode_Mav,              /*!< Multi-angle view Shot */
  eShotMode_Autorama,         /*!< Auto-panorama Shot */
  eShotMode_MultiMotionShot,  /*!< Multi-motion Shot */
  eShotMode_Panorama3D,       /*!< Panorama 3D Shot */
  eShotMode_Single3D,         /*!< Single Camera 3D Shot */
  eShotMode_EngShot,          /*!< Engineer Mode Shot */
  eShotMode_VideoSnapShot,    /*!< Video Snap Shot */
  eShotMode_FaceBeautyShotCc, /*!< Face beauty Shot */
  eShotMode_ZsdHdrShot,       /*!< ZSD with High-dynamic-range Shot */
  eShotMode_ZsdMfllShot,      /*!< ZSD with MFLL shot */
  eShotMode_MfllShot, /*!< Mfll Shot, only used in middleware v1.4 and later */
  eShotMode_ZsdVendorShot, /*!< Zero-shutter-delay Vendor Shot */
  eShotMode_VendorShot,    /*!< Vendor Shot */
  eShotMode_BMDNShot,      /*!< Dual cam denoise shot */
  eShotMode_MFHRShot,      /*!< Dual cam multi-frame high-resoulation shot */
  eShotMode_FusionShot,    /*!< Dual cam fusion shot */
  eShotMode_4CellRemosaicShot, /*!< 4-cell remosaic shot */
  eShotMode_DCMFShot,          /*!< Dual cam multi-frame shot */
  eShotMode_DCMFHdrShot,       /*!< Dual cam multi-frame shot (hdr+bokeh) */
  eShotMode_Undefined = 0xFF, /*!< Undefined shot, need to check the use flow */
};

/**
 * @enum EPipelineMode
 * @brief Pipeline Mode Preference
 *
 */
enum EPipelineMode {
  ePipelineMode_Null,
  ePipelineMode_Basic,
  ePipelineMode_Feature,
  ePipelineMode_Stereo,
  ePipelineMode_Eng,
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_MODES_H_
