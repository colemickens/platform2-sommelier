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
 * @file lmv_hal.h
 *
 * LMV Hal Header File
 *
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_LMV_LMV_HAL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_LMV_LMV_HAL_H_

#include <atomic>
#include <memory>
#include <mtkcam/def/UITypes.h>
#include <mtkcam/drv/iopipe/CamIO/Cam_QueryDef.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IHalCamIO.h>
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>
#include <mtkcam/feature/lmv/lmv_type.h>

using NSCam::IHalSensor;
using NSCam::IHalSensorList;
using NSCam::IImageBuffer;
using NSCam::SensorDynamicInfo;
using NSCam::SensorStaticInfo;
using NSCam::NSIoPipe::PORT_EISO;
using NSCam::NSIoPipe::PORT_IMGO;
using NSCam::NSIoPipe::PORT_LCSO;
using NSCam::NSIoPipe::PORT_RRZO;
using NSCam::NSIoPipe::PORT_RSSO;
using NSCam::NSIoPipe::NSCamIOPipe::BufInfo;
using NSCam::NSIoPipe::NSCamIOPipe::CAM_Pipeline_10BITS;
using NSCam::NSIoPipe::NSCamIOPipe::CAM_Pipeline_12BITS;
using NSCam::NSIoPipe::NSCamIOPipe::CAM_Pipeline_14BITS;
using NSCam::NSIoPipe::NSCamIOPipe::CAM_Pipeline_16BITS;
using NSCam::NSIoPipe::NSCamIOPipe::E_1_SEN;
using NSCam::NSIoPipe::NSCamIOPipe::E_2_SEN;
using NSCam::NSIoPipe::NSCamIOPipe::E_CAM_PipelineBitDepth_SEL;
using NSCam::NSIoPipe::NSCamIOPipe::E_CamHwPathCfg;
using NSCam::NSIoPipe::NSCamIOPipe::eCamHwPathCfg_Num;
using NSCam::NSIoPipe::NSCamIOPipe::eCamHwPathCfg_One_TG;
using NSCam::NSIoPipe::NSCamIOPipe::eCamHwPathCfg_Two_TG;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_GEN_MAGIC_NUM;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_GET_BIN_INFO;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_GET_DTwin_INFO;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_GET_HW_PATH_CFG;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_GET_QUALITY;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_GET_TG_OUT_SIZE;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_GET_UNI_SWITCH_STATE;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_SET_HW_PATH_CFG;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_SET_QUALITY;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_SET_RRZ_CBFP;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeCmd_UNI_SWITCH;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BS_RATIO;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_BURST_NUM;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_D_Twin;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_IQ_LEVEL;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_STRIDE_BYTE;
using NSCam::NSIoPipe::NSCamIOPipe::ENPipeQueryCmd_SUPPORT_PATTERN;
using NSCam::NSIoPipe::NSCamIOPipe::EPipe_PROCESSED_RAW;
using NSCam::NSIoPipe::NSCamIOPipe::EPipe_PURE_RAW;
using NSCam::NSIoPipe::NSCamIOPipe::EPipeSelect;
using NSCam::NSIoPipe::NSCamIOPipe::EPipeSelect_Normal;
using NSCam::NSIoPipe::NSCamIOPipe::EPipeSelect_NormalSv;
using NSCam::NSIoPipe::NSCamIOPipe::EPipeSignal;
using NSCam::NSIoPipe::NSCamIOPipe::EPipeSignal_AFDONE;
using NSCam::NSIoPipe::NSCamIOPipe::EPipeSignal_EOF;
using NSCam::NSIoPipe::NSCamIOPipe::EPipeSignal_SOF;
using NSCam::NSIoPipe::NSCamIOPipe::kPipeNormal;
using NSCam::NSIoPipe::NSCamIOPipe::kPipeTag_Out1_Tuning;
using NSCam::NSIoPipe::NSCamIOPipe::kPipeTag_Out2_Tuning;
using NSCam::NSIoPipe::NSCamIOPipe::NormalPipe_QueryInfo;
using NSCam::NSIoPipe::NSCamIOPipe::PipeTag;
using NSCam::NSIoPipe::NSCamIOPipe::PortInfo;
using NSCam::NSIoPipe::NSCamIOPipe::QBufInfo;
using NSCam::NSIoPipe::NSCamIOPipe::QInitParam;
using NSCam::NSIoPipe::NSCamIOPipe::QPortID;
using NSCam::NSIoPipe::NSCamIOPipe::ResultMetadata;
using NSCam::NSIoPipe::NSCamIOPipe::sCAM_QUERY_BURST_NUM;
using NSCam::NSIoPipe::NSCamIOPipe::sCAM_QUERY_D_Twin;
using NSCam::NSIoPipe::NSCamIOPipe::sCAM_QUERY_IQ_LEVEL;
using NSCam::NSIoPipe::NSCamIOPipe::sCAM_QUERY_SUPPORT_PATTERN;
using NSCam::NSIoPipe::NSCamIOPipe::V4L2IIOPipe;

/**
 *@brief LMV hal class used by scenario
 */
class VISIBILITY_PUBLIC LMVHal : public std::enable_shared_from_this<LMVHal> {
 public:
  /**
   *@brief LMVHal constructor
   */
  LMVHal() {}

  /**@brief Create LMVHal object
   *@param[in] userName : user name,i.e. who create LMV HAL object
   *@param[in] aSensorIdx : sensor index
   *@return
   *-LMVHal object
   */
  static std::shared_ptr<LMVHal> CreateInstance(char const* userName,
                                                const MUINT32& aSensorIdx);

  /**
   *@brief Initialization function
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Init(const MUINT32 eisFactor) = 0;

  /**
   *@brief Unitialization function
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 Uninit() = 0;

  /**
   *@brief Configure LMV
   *@details Use this API after pass1config and before pass1 start
   *@param[in] aLmvConfig : LMV config data
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 ConfigLMV(const LMV_HAL_CONFIG_DATA& aLmvConfig) = 0;

  /**
   *@brief Execute LMV
   *@param[in] QBufInfo : LMV result data
   *@return
   *-LMV_RETURN_NO_ERROR indicates success, otherwise indicates fail
   */
  virtual MINT32 DoLMVCalc(
      NSCam::NSIoPipe::NSCamIOPipe::QBufInfo const& pBufInfo) = 0;

  /**
       *@brief Get LMV algorithm result
       *@param[out] a_CMV_X_Int : LMV algo result of X-direction integer part
     //Deprecated
       *@param[out] a_CMV_X_Flt  : LMV algo result of X-direction float part
     //Deprecated
       *@param[out] a_CMV_Y_Int : LMV algo result of Y-direction integer part
     //Deprecated
       *@param[out] a_CMV_Y_Flt  : LMV algo result of Y-direction float part
     //Deprecated
       *@param[out] a_TarWidth    : LMV width crop size //Deprecated
       *@param[out] a_TarHeight   : LMV height crop size //Deprecated
       *@param[out] a_MVtoCenterX    : mv X to the left-top of center window in
     EIS domain
       *@param[out] a_MVtoCenterY    : mv Y to the left-top of center window in
     EIS domain
       *@param[out] a_isFromRRZ   : LMV input is from RRZ or not, value of  '1'
     means from RRZ, and sensor source must be RAW sensor value of  '0' means
     from others, and sensor source must be YUV sensor

     */
  virtual MVOID GetLMVResult(MUINT32* a_CMV_X_Int,
                             MUINT32* a_CMV_X_Flt,
                             MUINT32* a_CMV_Y_Int,
                             MUINT32* a_CMV_Y_Flt,
                             MUINT32* a_TarWidth,
                             MUINT32* a_TarHeight,
                             MINT32* a_MVtoCenterX,
                             MINT32* a_MVtoCenterY,
                             MUINT32* a_isFromRRZ) = 0;

  /**
   *@brief Get LMV GMV
   *@details The value is 256x
   *@param[out] aGMV_X : x-direction global motion vector between two frames
   *@param[out] aGMV_Y  : y-direction global motion vector between two frames
   */
  virtual MVOID GetGmv(MINT32* aGMV_X,
                       MINT32* aGMV_Y,
                       MUINT32* confX = NULL,
                       MUINT32* confY = NULL,
                       MUINT32* MAX_GMV = NULL) = 0;

  /**
   *@brief Get LMV HW support or not
   *@param[in] aSensorIdx : sensor index
   *@return
   *-MTRUE indicates LMV HW is supported, MFALSE indicates not supported
   */
  virtual MBOOL GetLMVSupportInfo(const MUINT32& aSensorIdx) = 0;

  virtual MINT32 GetBufLMV(std::shared_ptr<NSCam::IImageBuffer>* spBuf) = 0;
  virtual MINT32 NotifyLMV(
      NSCam::NSIoPipe::NSCamIOPipe::QBufInfo* pBufInfo) = 0;
  virtual MINT32 NotifyLMV(
      std::shared_ptr<NSCam::IImageBuffer> const& spBuf) = 0;

  /**
   *@brief Return LMV HW statistic result
   *@param[out] a_pLMV_Stat : EIS_STATISTIC_STRUCT
   */
  virtual MVOID GetLMVStatistic(EIS_STATISTIC_STRUCT* a_pLMV_Stat) = 0;

  /**
   *@brief  Get input width/height of LMV HW
   */
  virtual MVOID GetLMVInputSize(MUINT32* aWidth, MUINT32* aHeight) = 0;

  virtual NSCam::MSize QueryMinSize(MBOOL isEISOn,
                                    NSCam::MSize sensorSize,
                                    NSCam::MSize outputSize,
                                    NSCam::MSize requestSize,
                                    NSCam::MSize FovMargin) = 0;

  virtual MUINT32 GetLMVStatus() = 0;

 protected:
  /**
   *@brief LMVHal destructor
   */
  virtual ~LMVHal() {}
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_LMV_LMV_HAL_H_
