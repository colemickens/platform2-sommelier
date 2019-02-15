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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_FACEEFFECT_FACEDETECTION_FD4_0_FDVT_HAL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_FACEEFFECT_FACEDETECTION_FD4_0_FDVT_HAL_H_

#include <mtkcam/feature/FaceDetection/fd_hal_base.h>
#include <semaphore.h>
#include <mutex>
#include "MTKDetection.h"
#include <thread>
#include <FDIpcClientAdapter.h>

#define FD_SCALES 14
class MTKDetection;
/*******************************************************************************
 *
 ******************************************************************************/
class halFDVT : public halFDBase {
 public:
  //
  static halFDBase* getInstance(int openId);
  virtual void destroyInstance();
  //
  /////////////////////////////////////////////////////////////////////////
  //
  // halFDBase () -
  //! \brief FD Hal constructor
  //
  /////////////////////////////////////////////////////////////////////////
  explicit halFDVT(int openId);

  /////////////////////////////////////////////////////////////////////////
  //
  // ~mhalCamBase () -
  //! \brief mhal cam base descontrustor
  //
  /////////////////////////////////////////////////////////////////////////
  virtual ~halFDVT();

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDInit () -
  //! \brief init face detection
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDInit(MUINT32 fdW,
                           MUINT32 fdH,
                           MBOOL SWResizerEnable,
                           MUINT8 Current_mode,
                           MINT32 FldNum = 1);

  /////////////////////////////////////////////////////////////////////////
  //
  // halFDGetVersion () -
  //! \brief get FD version
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDGetVersion();

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDVTDo () -
  //! \brief process face detection
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDDo(struct FD_Frame_Parameters const& Param);

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDVTUninit () -
  //! \brief FDVT uninit
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDUninit();

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDVTGetFaceInfo () -
  //! \brief get face detection result
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDGetFaceInfo(MtkCameraFaceMetadata* fd_info_result);

  /////////////////////////////////////////////////////////////////////////
  //
  // mHalFDVTGetFaceResult () -
  //! \brief get face detection result
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDGetFaceResult(MtkCameraFaceMetadata* fd_result,
                                    MINT32 ResultMode = 1);
  /////////////////////////////////////////////////////////////////////////
  //
  // halFDYUYV2ExtractY () -
  //! \brief create Y Channel
  //
  /////////////////////////////////////////////////////////////////////////
  virtual MINT32 halFDYUYV2ExtractY(MUINT8* dstAddr,
                                    MUINT8* srcAddr,
                                    MUINT32 src_width,
                                    MUINT32 src_height);

 private:
  static void LockFTBuffer(void* arg);
  static void UnlockFTBuffer(void* arg);

  bool doHWFaceDetection(void* pCal_Data, int mem_fd, MUINT8* va);
  void dumpFDParam(MTKFDFTInitInfo const& FDFTInitInfo);

 protected:
  FDIpcClientAdapter* mpMTKFDVTObj;
  MUINT32 mFDW;
  MUINT32 mFDH;
  MUINT32 mBuffCount;
  MBOOL mInited;

  sem_t mSemFTLock;
  struct FTParam {
    MUINT8* dstAddr;
    MUINT8* srcAddr;
    MUINT8 ucPlane;
    MUINT32 src_width;
    MUINT32 src_height;
  };
  struct FTParam mFTParameter;

  MUINT32 mImage_width_array[FD_SCALES];
  MUINT32 mImage_height_array[FD_SCALES];
  MUINT32 mImageScaleTotalSize;
  MUINT8* mpImageScaleBuffer;
  MBOOL mEnableSWResizerFlag;
  MUINT8* mpImageVGABuffer;
  MINT32 mFdResult_Num;
  FD_RESULT mFdResult[15];
  MUINT32 mFTWidth;
  MUINT32 mFTHeight;
  MBOOL mCurrentMode;
  MUINT32 mDoDumpImage;
  float mFDFilterRatio;

  MUINT32 mUseUserScale;
  MUINT32 mUserScaleNum;
  MUINT32 mFrameCount;
  MUINT32 mDetectedFaceNum;
  FD_RESULT mFaceResult[15];
  MUINT32 mFDRefresh;
  MINT32 mMustScaleIndex;
  MINT32 mFDModelVer;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_FACEEFFECT_FACEDETECTION_FD4_0_FDVT_HAL_H_
