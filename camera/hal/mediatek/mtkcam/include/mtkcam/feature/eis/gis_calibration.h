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

/**
 * @file gis_calibration.h
 *
 * GIS Calibration Header File
 *
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EIS_GIS_CALIBRATION_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EIS_GIS_CALIBRATION_H_

#include <mtkcam/def/UITypes.h>
#include <mtkcam/feature/lmv/lmv_type.h>

/**
 *@brief GyroCalibration class used by scenario
 */
class GisCalibration {
 public:
  static GisCalibration* CreateInstance(char const* userName,
                                        const MUINT32& aSensorIdx,
                                        const MUINT32 eisFactor);

  /**
   *@brief Destroy EisHal object
   *@param[in] userName : user name,i.e. who destroy EIS HAL object
   */
  virtual MVOID DestroyInstance(char const* userName) = 0;

  /**
   *@brief Initialization function
   *@return
   *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Init() = 0;

  /**
   *@brief Unitialization function
   *@return
   *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Uninit() = 0;

  /**
   *@brief Configure EIS
   *@details Use this API after pass1/pass2 config and before pass1/pass2 start
   *@param[in] aEisPass : indicate pass1 or pass2
   *@param[in] aEisConfig : EIS config data
   *@return
   *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */

  virtual MINT32 ConfigCalibration(const LMV_HAL_CONFIG_DATA& aLMVConfig) = 0;

  /**
   *@brief DoCalibration
   *@param[in] aEisPass : indicate pass1 or pass2
   *@param[in] apEisConfig : EIS config data, mainly for pass2
   *@param[in] aTimeStamp : time stamp of pass1 image
   *@return
   *-EIS_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 DoCalibration(LMV_HAL_CONFIG_DATA* apLMVConfig = NULL,
                               MINT64 aTimeStamp = -1,
                               MINT64 aExpTime = 0) = 0;

 protected:
  /**
   *@brief GisCalibration constructor
   */
  GisCalibration() {}
  /**
   *@brief GisCalibration destructor
   */
  virtual ~GisCalibration() {}
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EIS_GIS_CALIBRATION_H_
