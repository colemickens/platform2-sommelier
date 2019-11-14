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
 * @file lmv_drv_imp.h
 *
 * LMV Driver Implementation Header File
 *
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_LMV_LMV_DRV_IMP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_LMV_LMV_DRV_IMP_H_

#include <lmv_drv.h>
#include <lmv_tuning.h>
// MTKCAM/V4L2 with IPC usage
#include <mtkcam/drv/iopipe/CamIO/V4L2IHalCamIO.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>

#if MTKCAM_HAVE_SANDBOX_SUPPORT
#include <mtkcam/v4l2/IPCIHalSensor.h>
#include <mtkcam/v4l2/V4L2HwEventMgr.h>
#include <mtkcam/v4l2/V4L2P13ACallback.h>
#include <mtkcam/v4l2/V4L2SensorMgr.h>
#include <mtkcam/v4l2/V4L2SttPipeMgr.h>
#include <mtkcam/v4l2/V4L2TuningPipeMgr.h>
#endif

#include <memory>

/**
 *@brief LMV HW register
 */
struct LMV_REG_INFO {
  MUINT32 reg_lmv_prep_me_ctrl1 = 0;  // CAM_LMV_PREP_ME_CTRL1
  MUINT32 reg_lmv_prep_me_ctrl2 = 0;  // CAM_LMV_PREP_ME_CTRL2
  MUINT32 reg_lmv_lmv_th = 0;         // CAM_LMV_LMV_TH
  MUINT32 reg_lmv_fl_offset = 0;      // CAM_LMV_FL_OFFSET
  MUINT32 reg_lmv_mb_offset = 0;      // CAM_LMV_MB_OFFSET
  MUINT32 reg_lmv_mb_interval = 0;    // CAM_LMV_MB_INTERVAL
  MUINT32 reg_lmv_gmv = 0;            // CAM_LMV_GMV, not use
  MUINT32 reg_lmv_err_ctrl = 0;       // CAM_LMV_ERR_CTRL, not use
  MUINT32 reg_lmv_image_ctrl = 0;     // CAM_LMV_IMAGE_CTRL
};

/**
 *@brief Implementation of LMVDrv class
 */
class LMVDrvImp : public LMVDrv {
 public:
  /**
   *@brief Create LMVDrv object
   *@param[in] sensorIdx : sensor index
   *@return
   *-LMVDrvImp object
   */
  static std::shared_ptr<LMVDrv> CreateDrvImpInstance(
      const MUINT32& aSensorIdx);

  /**
   *@brief Initial function
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Init(NSCam::MSize sensorSize, NSCam::MSize rrzoSize);

  /**
   *@brief Uninitial function
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Uninit();

  virtual void LmvParasInit(LMV_INPUT_MSG input);

  /**
   *@brief Configure LMV and related register value
   *@param[in] aSensorTg : sensor TG info for debuging usage
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 ConfigLMVReg(const MUINT32& aSensorTg);

  /**
   *@brief Get first frame or not
   *@return
   *-0 : not first frame, 1 : first frame
   */
  virtual MUINT32 GetFirstFrameInfo();

  /**
   *@brief Get first frame or not
   *@return
   *-0 : not 2-pixel mode, 1 : 2-pixel mode
   */
  virtual MUINT32 Get2PixelMode();

  /**
   *@brief  Get input width/height of LMV HW
   */
  virtual MVOID GetLMVInputSize(MUINT32* aWidth, MUINT32* aHeight);

  /**
   *@brief  Return LMV HW setting of EOS_OP_HORI
   *@return
   *-EOS_OP_HORI
   */
  virtual MUINT32 GetLMVDivH();

  /**
   *@brief  Return LMV HW setting of LMV_OP_VERT
   *@return
   *-LMV_OP_VERT
   */
  virtual MUINT32 GetLMVDivV();

  /**
   *@brief  Return  LMV HW setting of MAX gmv range
   *@return
   *-32 or 64
   */
  virtual MUINT32 GetLMVMaxGmv();

  /**
   *@brief  Return Total MB number
   *@return
   *-MBNum_V * MB_Num_H
   */
  virtual MUINT32 GetLMVMbNum();

  /**
   *@brief Get LMV HW support or not
   *@param[in] aSensorIdx : sensor index
   *@return
   *-MTRUE indicates LMV HW is supported, MFALSE indicates not supported
   */
  virtual MBOOL GetLMVSupportInfo(const MUINT32& aSensorIdx);

  /**
   *@brief Get timestamp for EisPlusAlgo debuging
   *@return
   *-one timestamp of LMVO
   */
  virtual MINT64 GetTsForAlgoDebug();

  /**
   *@brief  Get statistic of LMV HW
   *@param[in,out] apLmvStat : EIS_STATISTIC_T struct
   *@param[in] aTimeStamp : time stamp of  pass1 image
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 GetLmvHwStatistic(MINTPTR bufferVA,
                                   EIS_STATISTIC_STRUCT* apLmvStat);

  /**
   *@brief Do boundary check
   *@param[in,out] a_input : input number
   *@param[in] upBound : upper bound
   *@param[in] lowBound : lower bound
   */
  MVOID BoundaryCheck(MUINT32* aInput,
                      const MUINT32& upBound,
                      const MUINT32& lowBound);

  //------------------------------------------------------------------------

  // LMV and related register setting
  LMV_REG_INFO mLmvRegSetting;

  // member variable
  MUINT32 mIsConfig = 0;
  MUINT32 mIsFirst = 1;
  MUINT32 mIs2Pixel = 0;
  MUINT32 mTotalMBNum = 0;
  MUINT32 mImgWidth = 0;
  MUINT32 mImgHeight = 0;
  MUINT32 mLmvDivH = 0;
  MUINT32 mLmvDivV = 0;
  MUINT32 mMaxGmv = LMV_MAX_GMV_DEFAULT;
  LMV_SENSOR_ENUM mSensorType = LMV_NULL_SENSOR;

  MINT32 mSensor_Width = 0;     // sensor
  MINT32 mSensor_Height = 0;    // sensor
  MINT32 mRRZ_in_Width = 0;     // RRZ in width
  MINT32 mRRZ_in_Height = 0;    // RRZ in height
  MINT32 mRrz_crop_Width = 0;   // sensor crop
  MINT32 mRrz_crop_Height = 0;  // sensor crop
  MINT32 mRrz_crop_X = 0;
  MINT32 mRrz_crop_Y = 0;
  MINT32 mRrz_scale_Width = 0;   // RRZ output
  MINT32 mRrz_scale_Height = 0;  // RRZ output

 public:
  /**
   *@brief  LMVDrvImp constructor
   *@param[in] aSensorIdx : sensor index
   */
  explicit LMVDrvImp(const MUINT32& aSensorIdx);

  /**
   *@brief  LMVDrvImp destructor
   */
  ~LMVDrvImp();

 private:
  /**
   *@brief Perform complement2 on value according to digit
   *@param[in] value : value need to do complement2
   *@param[in] digit : indicate how many digits in value are valid
   *@return
   *-value after doing complement2
   */
  MINT32 Complement2(MUINT32 value, MUINT32 digit);

  /**
   *@brief Get timestamp as ISP driver gave MW
   *@param[in] aSec : second
   *@param[in] aUs : micro second
   */
  MINT64 GetTimeStamp(const MUINT32& aSec, const MUINT32& aUs);

  /**************************************************************************/

  std::atomic_int mUsers;
  mutable std::mutex mLock;

  // member variable
  MUINT32 mSensorIdx = 0;
  MUINT32 mLmvoIsFirst = 1;
  MBOOL mLmvHwSupport = MTRUE;
  MINT64 mTsForAlgoDebug = 0;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_LMV_LMV_DRV_IMP_H_
