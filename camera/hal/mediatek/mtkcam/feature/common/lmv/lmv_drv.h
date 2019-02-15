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
 * @file lmv_drv.h
 *
 * LMV Driver Header File
 *
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_LMV_LMV_DRV_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_LMV_LMV_DRV_H_

#include <mtkcam/feature/lmv/lmv_type.h>
#include <atomic>
#include <memory>

/**
 *@brief LMV driver class used by LMV_Hal
 */
class LMVDrv : public std::enable_shared_from_this<LMVDrv> {
 public:
  /**
   *@brief  LMVDrv constructor
   */
  LMVDrv() {}

  /**
   *@brief LMVDrv destructor
   */
  virtual ~LMVDrv() {}

  /**
   *@brief Create LMVDrv object
   *@param[in] aSensorIdx : sensor index
   *@return
   *-LMVDrv object
   */
  std::shared_ptr<LMVDrv> CreateInstance(const MUINT32& aSensorIdx);

  /**
   *@brief Initial function
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Init() = 0;

  /**
   *@brief Uninitial function
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Uninit() = 0;

  /**
   *@brief Configure LMV and related register value
   *@param[in] aSce : LMV configure scenario
   *@param[in] aSensorTg : sensor TG info for debuging usage
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 ConfigLMVReg(const MUINT32& aSensorTg) = 0;

  /**
   *@brief Get first frame or not
   *@return
   *-1 : not first frame, 0 : first frame
   */
  virtual MUINT32 GetFirstFrameInfo() = 0;

  /**
   *@brief Get first frame or not
   *@return
   *-0 : not 2-pixel mode, 1 : 2-pixel mode
   */
  virtual MUINT32 Get2PixelMode() = 0;

  /**
   *@brief  Get input width/height of LMV HW
   */
  virtual MVOID GetLMVInputSize(MUINT32* aWidth, MUINT32* aHeight) = 0;

  /**
   *@brief  Return  LMV HW setting of EOS_OP_HORI
   *@return
   *-EOS_OP_HORI
   */
  virtual MUINT32 GetLMVDivH() = 0;

  /**
   *@brief  Return  LMV HW setting of LMV_OP_VERT
   *@return
   *-LMV_OP_VERT
   */
  virtual MUINT32 GetLMVDivV() = 0;

  /**
   *@brief  Return  LMV HW setting of MAX gmv range
   *@return
   *-32 or 64
   */
  virtual MUINT32 GetLMVMaxGmv() = 0;

  /**
   *@brief  Return  Total MB number
   *@return
   *-MBNum_V * MB_Num_H
   */
  virtual MUINT32 GetLMVMbNum() = 0;

  /**
   *@brief Get LMV HW support or not
   *@param[in] aSensorIdx : sensor index
   *@return
   *-MTRUE indicates LMV HW is supported, MFALSE indicates not supported
   */
  virtual MBOOL GetLMVSupportInfo(const MUINT32& aSensorIdx) = 0;

  /**
   *@brief Get timestamp for EisPlusAlgo debuging
   *@return
   *-one timestamp of LMVO
   */
  virtual MINT64 GetTsForAlgoDebug() = 0;

  /**
   *@brief  Get statistic of LMV HW
   *@param[in,out] apLmvStat : EIS_STATISTIC_T struct
   *@param[in] aTimeStamp : time stamp of  pass1 image
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 GetLmvHwStatistic(MINTPTR bufferVA,
                                   EIS_STATISTIC_STRUCT* apLmvStat) = 0;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_LMV_LMV_DRV_H_
