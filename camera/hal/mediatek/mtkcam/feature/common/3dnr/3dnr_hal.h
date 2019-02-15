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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_3DNR_3DNR_HAL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_3DNR_3DNR_HAL_H_

#include <atomic>
#include <common/3dnr/3dnr_hal_base.h>
#include <memory>
#include <mtkcam/aaa/IIspMgr.h>
#include <mtkcam/def/common.h>
#include <queue>

using NSCam::NSIoPipe::NSPostProc::hal3dnrBase;
using NSCam::NSIoPipe::NSPostProc::NR3D_ERROR_FORCE_SKIP;
using NSCam::NSIoPipe::NSPostProc::NR3D_ERROR_FRAME_SIZE_CHANGED;
using NSCam::NSIoPipe::NSPostProc::NR3D_ERROR_INPUT_SRC_CHANGE;
using NSCam::NSIoPipe::NSPostProc::NR3D_ERROR_INVALID_PARAM;
using NSCam::NSIoPipe::NSPostProc::NR3D_ERROR_NONE;
using NSCam::NSIoPipe::NSPostProc::NR3D_ERROR_NOT_SUPPORT;
using NSCam::NSIoPipe::NSPostProc::NR3D_ERROR_UNDER_ISO_THRESHOLD;
using NSCam::NSIoPipe::NSPostProc::NR3D_STATE_ENUM;
using NSCam::NSIoPipe::NSPostProc::NR3D_STATE_PREPARING;
using NSCam::NSIoPipe::NSPostProc::NR3D_STATE_STOP;
using NSCam::NSIoPipe::NSPostProc::NR3D_STATE_WORKING;
using NSCam::NSIoPipe::NSPostProc::NR3DHALParam;
using NSCam::NSIoPipe::NSPostProc::NR3DParam;
using NSCam::NSIoPipe::NSPostProc::NR3DRSCInfo;
/******************************************************************************
 *
 ******************************************************************************/
struct hal3dnrSavedFrameInfo {
  MUINT32 CmvX;  // Keep track of CMV X.
  MUINT32 CmvY;  // Keep track of CMV Y.

  MBOOL isCRZUsed;
  MBOOL isIMGO;
  MBOOL isBinning;

  hal3dnrSavedFrameInfo()
      : CmvX(0),
        CmvY(0),
        isCRZUsed(MFALSE),
        isIMGO(MFALSE),
        isBinning(MFALSE) {}
};

class hal3dnrBase_v2 {
 public:
  virtual ~hal3dnrBase_v2() {}

  virtual MBOOL savedFrameInfo(const NR3DHALParam& nr3dHalParam) = 0;
  virtual MBOOL handle3DNROnOffPolicy(const NR3DHALParam& nr3dHalParam) = 0;
  virtual MBOOL handleAlignVipiIMGI(const NR3DHALParam& nr3dHalParam,
                                    NR3DParam* outNr3dParam) = 0;
  virtual MBOOL configNR3D(const NR3DHALParam& nr3dHalParam,
                           const NR3DParam& Nr3dParam) = 0;

 protected:
  hal3dnrSavedFrameInfo mCurSavedFrameInfo;
  hal3dnrSavedFrameInfo mPreSavedFrameInfo;
};

class Hal3dnr : public hal3dnrBase, public hal3dnrBase_v2 {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Operations.
  //
  static std::shared_ptr<hal3dnrBase> getInstance();
  static std::shared_ptr<hal3dnrBase> getInstance(char const* userName,
                                                  const MUINT32 sensorIdx);

  //
  /////////////////////////////////////////////////////////////////////////
  //
  // halFDBase () -
  //! \brief 3dnr Hal constructor
  //
  /////////////////////////////////////////////////////////////////////////
  Hal3dnr();
  /////////////////////////////////////////////////////////////////////////
  //
  // halFDBase () -
  //! \brief 3dnr Hal constructor
  //
  /////////////////////////////////////////////////////////////////////////
  explicit Hal3dnr(const MUINT32 sensorIdx);

  /////////////////////////////////////////////////////////////////////////
  //
  // ~mhalCamBase () -
  //! \brief mhal cam base descontrustor
  //
  /////////////////////////////////////////////////////////////////////////
  virtual ~Hal3dnr();

  /**
   * @brief init the 3ndr hal
   *
   * @details
   * - Prepare all 3dnr variable and set to init status.
   * - Create NR3D struct object.
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL init(MINT32 force3DNR = 0);

  /**
   * @brief uninit the 3ndr hal
   *
   * @details
   * - Set all variable value to init status.
   * - Delete NR3D object.
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL uninit();

  /**
   * @brief prepare the 3ndr hal
   *
   * @details
   * - Do 3DNR State Machine operation.
   * - Reset m3dnrErrorStatus.
   * - 3DNR HW module on/off according ISO value.
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL prepare(MUINT32 frameNo, MINT32 iso);

  /**
   * @brief setCMVMode
   *
   * @details
   * - set 3DNR GMV/CMV mode
   * @note
   *
   * @return
   * - NONE
   */
  virtual MVOID setCMVMode(MBOOL useCMV);

  /**
   * @brief setGMV to 3dnr hal
   *
   * @details
   * - 3DNR GMV Calculation according to input parameters that may from pipeline
   * metadata.
   * - Check GMV value is valid to do 3dnr to not.
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL setGMV(MUINT32 frameNo,
                       MINT32 gmvX,
                       MINT32 gmvY,
                       MINT32 cmvX_Int,
                       MINT32 cmvY_Int);

  /**
   * @brief compare IMG3O size with previous frame
   *
   * @details
   * - Check IMG3O buffer size with previous frame buffer.
   * - Update State Machine when size is different.
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL checkIMG3OSize(MUINT32 frameNo, MUINT32 imgiW, MUINT32 imgiH);

  /**
   * @brief Check can config vipi or not and set all related parameters to
   * Nr3dParam
   *
   * @details
   * - Check VIPI buffer is exist according to input parameter isVIPIIn.
   * - Calculate VIPI start addr offset.
   * - Calculate VIPI valid region w/h.
   * - Check State Machine and set the right Nr3dParam.
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL setVipiParams(MBOOL isVIPIIn,
                              MUINT32 vipiW,
                              MUINT32 vipiH,
                              MINT imgFormat,
                              size_t stride);

  /**
   * @brief Get 3dnr parametes
   *
   * @details
   * - Get Nr3dParam from 3dnr hal.
   * - Save current image size for next frame compared.
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL get3dnrParams(
      MUINT32 frameNo,
      MUINT32 imgiW,
      MUINT32 imgiH,
      std::shared_ptr<NR3DParam>* pNr3dParam);  // deprecated
  virtual MBOOL get3dnrParams(MUINT32 frameNo,
                              MUINT32 imgiW,
                              MUINT32 imgiH,
                              std::shared_ptr<NR3DParam> nr3dParam);

  /**
   * @brief Check 3dnr hal State Machine is equal to input status parameter or
   * not
   *
   * @details
   * - As function name, check 3dnr hal State Machine is equal to input status
   * parameter or not.
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure, and an error code can be retrived by
   * getLastErrorCode().
   */
  virtual MBOOL checkStateMachine(NR3D_STATE_ENUM status);

  /**
   * @brief Wrap several APIs into one compact API
   *
   * @details
   * - prepare all necessary parameters and set NR3D register into NR3D HW by
   * ISP
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure.
   */
  virtual MBOOL do3dnrFlow(void* pTuningData,
                           MBOOL useCMV,
                           NS3Av3::MRect const& dst_resizer_rect,
                           NSCam::NR3D::NR3DMVInfo const& GMVInfo,
                           NSCam::IImageBuffer* pIMGBufferVIPI,
                           MINT32 iso,
                           MUINT32 requestNo,
                           std::shared_ptr<NS3Av3::IHal3A> p3A);

  /**
   * @brief Wrap several APIs into one compact API V2
   *
   * @details
   * - prepare all necessary parameters and set NR3D register into NR3D HW by
   * ISP
   * @note
   *
   * @return
   * - MTRUE indicates success;
   * - MFALSE indicates failure.
   */
  virtual MBOOL do3dnrFlow_v2(const NR3DHALParam& nr3dHalParam);
  virtual MBOOL savedFrameInfo(const NR3DHALParam& nr3dHalParam);
  virtual MBOOL handle3DNROnOffPolicy(const NR3DHALParam& nr3dHalParam);
  virtual MBOOL handleAlignVipiIMGI(const NR3DHALParam& nr3dHalParam,
                                    NR3DParam* outNr3dParam);
  virtual MBOOL configNR3D(const NR3DHALParam& nr3dHalParam,
                           const NR3DParam& Nr3dParam);
  virtual MBOOL updateISPMetadata(
      NSCam::IMetadata* pMeta_InHal,
      const NSCam::NR3D::NR3DTuningInfo& tuningInfo);

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Variables.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 private:
  MINT32 mLogLevel;
  MINT32 mForce3DNR;  // hal force support 3DNR
  MBOOL mSupportZoom3DNR;

  MUINT32 mPrevFrameWidth;   // Keep track of previous frame width.
  MUINT32 mPrevFrameHeight;  // Keep track of previous frame height.
  MINT32 mNmvX;
  MINT32 mNmvY;
  MUINT32 mCmvX;      // Current frame CMV X.
  MUINT32 mCmvY;      // Current frame CMV Y.
  MUINT32 mPrevCmvX;  // Keep track of previous CMV X.
  MUINT32 mPrevCmvY;  // Keep track of previous CMV Y.
  MINT32 m3dnrGainZeroCount;
  MUINT32 m3dnrErrorStatus;
  NR3D_STATE_ENUM m3dnrStateMachine;

  std::shared_ptr<NR3DParam> mpNr3dParam;  // For NR3D struct in PostProc.
  const MUINT32 mSensorIdx;
  std::atomic_int mUsers;
  mutable std::mutex mLock;

  MBOOL mIsCMVMode;
  NS3Av3::IIspMgrIPC* mIIspMgr;
  // adjInput: NR3DCustom::AdjustmentInput*.
  // We don't include the custom header here
  MBOOL fillGyroForAdjustment(void* adjInput);
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_3DNR_3DNR_HAL_H_
