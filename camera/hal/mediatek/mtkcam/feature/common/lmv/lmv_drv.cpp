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

//! \file  lmv_drv.cpp

#include <camera/hal/mediatek/mtkcam/feature/common/lmv/lmv_drv.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <cctype>
#include <errno.h>

#include <mtkcam/feature/eis/eis_type.h>
#include <mtkcam/drv/iopipe/CamIO/Cam_Notify.h>
#include <mtkcam/drv/IHalSensor.h>

#include <lmv_drv_imp.h>
#include <memory>

/***************************************************************************
 * Define Value
 ****************************************************************************/

#define LMV_DRV_DEBUG

#undef LOG_TAG
#define LOG_TAG "LMVDrv"
#include <mtkcam/utils/std/Log.h>

#define LMV_DRV_NAME "LMVDrv"
#define LMV_DRV_DUMP "vendor.debug.LMVDrv.dump"

/*******************************************************************************
 * Global variable
 *****************************************************************************/
static MINT32 g_debugDump = 1;

std::shared_ptr<LMVDrv> LMVDrv::CreateInstance(const MUINT32& aSensorIdx) {
  return LMVDrvImp::CreateDrvImpInstance(aSensorIdx);
}

std::shared_ptr<LMVDrv> LMVDrvImp::CreateDrvImpInstance(
    const MUINT32& aSensorIdx) {
  CAM_LOGD("aSensorIdx(%u)", aSensorIdx);
  return std::make_shared<LMVDrvImp>(aSensorIdx);
}
static NSCam::NSIoPipe::NSCamIOPipe::IV4L2PipeFactory* getNormalPipeModule() {
  static auto* p_v4l2_factory =
      NSCam::NSIoPipe::NSCamIOPipe::IV4L2PipeFactory::get();
  MY_LOGE_IF(!p_v4l2_factory, "IV4L2PipeFactory::get() fail");

  return p_v4l2_factory;
}

LMVDrvImp::LMVDrvImp(const MUINT32& aSensorIdx) : LMVDrv() {
  mSensorIdx = aSensorIdx;
  mUsers = 0;
}

LMVDrvImp::~LMVDrvImp() {}

MINT32 LMVDrvImp::Init(NSCam::MSize sensorSize, NSCam::MSize rrzoSize) {
  MINT32 err = LMV_RETURN_NO_ERROR;

  std::lock_guard<std::mutex> _l(mLock);
  CAM_LOGD("sensorSize %dx%d, rrzoSize %dx%d", sensorSize.w, sensorSize.h,
           rrzoSize.w, rrzoSize.h);
  LMV_INPUT_MSG lmvInputParams;
  lmvInputParams.sRMXOut = rrzoSize;
  lmvInputParams.sHBINOut = rrzoSize;
  lmvInputParams.sTGOut = sensorSize;
  lmvInputParams.bYUVFmt = false;
  lmvInputParams.pixMode = 0;
  LmvParasInit(lmvInputParams);
  if (mUsers > 0) {
    std::atomic_fetch_add_explicit(&mUsers, 1, std::memory_order_release);
    CAM_LOGD("mSensorIdx(%u) has one more users", mSensorIdx);
    return LMV_RETURN_NO_ERROR;
  }
  g_debugDump = ::property_get_int32(LMV_DRV_DUMP, 0);

  CAM_LOGD("mSensorIdx(%u) init", mSensorIdx);
  auto pModule = getNormalPipeModule();

  if (!pModule) {
    CAM_LOGE("getNormalPipeModule() fail");
    return LMV_RETURN_NULL_OBJ;
  }

  {
    int status = 0;

    //  Select version
    size_t count = 0;
    MUINT32 const* version = NULL;
    status = pModule->get_sub_module_api_version(&version, &count);
    if (status < 0 || !count || !version) {
      CAM_LOGE(
          "[%d] INormalPipeModule::get_sub_module_api_version - err:%#x "
          "count:%zu version:%p",
          mSensorIdx, status, count, version);
      return LMV_RETURN_NULL_OBJ;
    }

    MUINT32 selectedVersion = *(version + count - 1);  // Select max. version
    CAM_LOGD("[%d] count:%zu Selected CamIO Version:%0#x", mSensorIdx, count,
             selectedVersion);

    //  Create CamIO

    getNormalPipeModule()->getSubModule(
        NSCam::NSIoPipe::NSCamIOPipe::kPipeNormal, mSensorIdx, LMV_DRV_NAME,
        selectedVersion);
  }

  std::atomic_fetch_add_explicit(&mUsers, 1, std::memory_order_release);

  return err;
}

MINT32 LMVDrvImp::Uninit() {
  std::lock_guard<std::mutex> _l(mLock);

  if (mUsers <= 0) {  // No more users
    CAM_LOGD("mSensorIdx(%u) has 0 user", mSensorIdx);
    return LMV_RETURN_NO_ERROR;
  }

  MINT32 err = LMV_RETURN_NO_ERROR;

  // >= one user
  std::atomic_fetch_sub_explicit(&mUsers, 1, std::memory_order_release);

  if (mUsers == 0) {
    CAM_LOGD("mSensorIdx(%u) uninit", mSensorIdx);

    memset(&mLmvRegSetting, 0, sizeof(LMV_REG_INFO));
    mIsConfig = 0;
    mIsFirst = 1;
    mIs2Pixel = 0;
    mTotalMBNum = 0;
    mImgWidth = 0;
    mImgHeight = 0;
    mLmvDivH = 0;
    mLmvDivV = 0;
    mMaxGmv = LMV_MAX_GMV_DEFAULT;
    mSensorType = LMV_NULL_SENSOR;
    mLmvoIsFirst = 1;

  } else {
    CAM_LOGD("mSensorIdx(%u) has one more users ", mSensorIdx);
  }
  mTsForAlgoDebug = 0;

  return err;
}

void LMVDrvImp::LmvParasInit(LMV_INPUT_MSG input) {
  CAM_LOGD("TG(%u,%u),RMX(%u,%u),HBIN(%u,%u),pixelMode(%d),YUV(%d)",
           input.sTGOut.w, input.sTGOut.h, input.sRMXOut.w, input.sRMXOut.h,
           input.sHBINOut.w, input.sHBINOut.h, input.pixMode, input.bYUVFmt);

  if (input.bYUVFmt == MFALSE) {  // RAW sensor specific register
    MUINT32 tempW = 0, tempH = 0;

    tempW = input.sRMXOut.w;
    tempH = input.sRMXOut.h;

    //> imgo only
    if (input.sRMXOut.w == 0 && input.sRMXOut.h == 0) {
      if (mIsFirst == 1) {
        CAM_LOGD("imgo only");
      }
      tempW = input.sHBINOut.w;
      tempH = input.sHBINOut.h;
    }

    //> pixel mode: 0(None) or 1(2-Pixel mode) or 2(Quad-Pixel)
    if ((input.pixMode > ePixMode_NONE) && (input.pixMode < ePixMode_4)) {
      if (mIsFirst == 1) {
        CAM_LOGD("pixel mode: %d", input.pixMode);
      }

      tempW = tempW >> input.pixMode;
      if (tempW != (MUINT32)input.sHBINOut.w) {
        CAM_LOGW("RRZ shift HDS(%d) is different to HBIN(%d)", tempW,
                 input.sHBINOut.w);
        tempW = input.sHBINOut.w;
      }
      mIs2Pixel = input.pixMode;
    } else {
      mIs2Pixel = 0;
    }

    if (mImgWidth != tempW || mImgHeight != tempH) {
      CAM_LOGD("(1)first:new(%u,%u),old(%u,%u)", tempW, tempH, mImgWidth,
               mImgHeight);
      mIsFirst = 1;
    } else {
      mIsFirst = 0;
    }

    mImgWidth = tempW;
    mImgHeight = tempH;
    mSensorType = LMV_RAW_SENSOR;
  } else if (input.bYUVFmt == MTRUE) {  // YUV sensor specific register
    //> get TG size
    if (mImgWidth != (MUINT32)input.sTGOut.w ||
        mImgHeight != (MUINT32)input.sTGOut.h) {
      CAM_LOGD("(2)first:new(%u,%u),old(%u,%u)", input.sTGOut.w, input.sTGOut.h,
               mImgWidth, mImgHeight);
      mIsFirst = 1;
    } else {
      mIsFirst = 0;
    }

    mImgWidth = input.sTGOut.w;
    mImgHeight = input.sTGOut.h;
    mSensorType = LMV_YUV_SENSOR;
  }

  //====== Setting Depend on Image Size ======

  MUINT32 win_numV = 3, win_numH = 2;
  MUINT32 max_range = 0;

  if (mImgWidth > HD_8M_WIDTH) {
    mLmvDivH = 4;
  } else if (mImgWidth > HD_720P_WIDTH) {
    mLmvDivH = 2;
  } else {
    mLmvDivH = 1;
  }

  if (mImgWidth > D1_WIDTH) {
    win_numH = 4;
  } else if (mImgWidth > CIF_WIDTH) {
    win_numH = 4;
  } else {
    win_numH = 2;
  }

  // vertical
  if (mImgHeight > HD_8M_HEIGHT) {
    mLmvDivV = 4;
  } else if (mImgHeight > HD_720P_HEIGHT) {
    mLmvDivV = 2;
  } else {
    mLmvDivV = 1;
  }

  if (mImgHeight > D1_HEIGHT) {
    win_numV = 8;
  } else if (mImgHeight > CIF_HEIGHT) {
    win_numV = 4;
  } else {
    win_numV = 3;
  }
  mMaxGmv = max_range;
  mTotalMBNum = win_numH * win_numV;
  CAM_LOGD(
      "org (w,h)=(%d,%d), (DivH, DivV)=(%d,%d), (winH, winV)=(%d,%d), "
      "MaxGmv(%d), TotalMBNum(%d)",
      mImgWidth, mImgHeight, mLmvDivH, mLmvDivV, win_numH, win_numV, max_range,
      mTotalMBNum);
}

MINT32 LMVDrvImp::ConfigLMVReg(const MUINT32& aSensorTg) {
  return LMV_RETURN_NO_ERROR;
}

MUINT32 LMVDrvImp::GetFirstFrameInfo() {
  return mIsFirst ? 0 : 1;
}

MUINT32 LMVDrvImp::Get2PixelMode() {
  return mIs2Pixel;
}

MVOID LMVDrvImp::GetLMVInputSize(MUINT32* aWidth, MUINT32* aHeight) {
  *aWidth = mImgWidth;
  *aHeight = mImgHeight;
}

MUINT32 LMVDrvImp::GetLMVDivH() {
  if (g_debugDump >= 1) {
    CAM_LOGD("mLmvDivH(%u)", mLmvDivH);
  }
  return mLmvDivH;
}

MUINT32 LMVDrvImp::GetLMVDivV() {
  if (g_debugDump >= 1) {
    CAM_LOGD("mLmvDivV(%u)", mLmvDivV);
  }
  return mLmvDivV;
}

MUINT32 LMVDrvImp::GetLMVMaxGmv() {
  if (g_debugDump >= 1) {
    CAM_LOGD("mMaxGmv(%u)", mMaxGmv);
  }
  return mMaxGmv;
}

MUINT32 LMVDrvImp::GetLMVMbNum() {
  if (g_debugDump >= 1) {
    CAM_LOGD("mTotalMBNum(%u)", mTotalMBNum);
  }
  return mTotalMBNum;
}

MBOOL LMVDrvImp::GetLMVSupportInfo(const MUINT32& aSensorIdx) {
  mLmvHwSupport = MFALSE;
  return MTRUE;
}

MINT64 LMVDrvImp::GetTsForAlgoDebug() {
  return mTsForAlgoDebug;
}

MINT32 LMVDrvImp::GetLmvHwStatistic(MINTPTR bufferVA,
                                    EIS_STATISTIC_STRUCT* apLmvStat) {
  if (!bufferVA) {
    CAM_LOGD("bufferVA(%p) is NULL!!!", (void*)bufferVA);
    return LMV_RETURN_EISO_MISS;
  }
  MUINT32* pLmvoAddr = reinterpret_cast<MUINT32*>(bufferVA);

  for (int i = 0; i < LMV_MAX_WIN_NUM; ++i) {
    if (i != 0) {
      pLmvoAddr += 2;  // 64bits(8bytes)
    }

    apLmvStat->i4LMV_X2[i] = Complement2(*pLmvoAddr & 0x1F, 5);  // [0:4]
    apLmvStat->i4LMV_Y2[i] =
        Complement2(((*pLmvoAddr & 0x3E0) >> 5), 5);             // [5:9]
    apLmvStat->SAD[i] = (*pLmvoAddr & 0x7FC00) >> 10;            // [10:18]
    apLmvStat->NewTrust_X[i] = (*pLmvoAddr & 0x03F80000) >> 19;  // [19:25]
    apLmvStat->NewTrust_Y[i] =
        ((*pLmvoAddr & 0xFC000000) >> 26) +
        ((*(pLmvoAddr + 1) & 0x00000001) << 6);  // [26:32]
    apLmvStat->i4LMV_X[i] = Complement2(((*(pLmvoAddr + 1) & 0x00003FFE) >> 1),
                                        13);  // [33:45] -> [1:13]
    apLmvStat->i4LMV_Y[i] = Complement2(((*(pLmvoAddr + 1) & 0x07FFC000) >> 14),
                                        13);  // [46:58] -> [14:26]
    apLmvStat->SAD2[i] = 0;
    apLmvStat->AVG_SAD[i] = 0;

    if (g_debugDump == 3) {
      CAM_LOGD("LMV[%d]Addr(%p)=lmv(%d,%d),lmv2(%d,%d),trust(%d,%d),sad(%d)", i,
               pLmvoAddr, apLmvStat->i4LMV_X[i], apLmvStat->i4LMV_Y[i],
               apLmvStat->i4LMV_X2[i], apLmvStat->i4LMV_Y2[i],
               apLmvStat->NewTrust_X[i], apLmvStat->NewTrust_Y[i],
               apLmvStat->SAD[i]);
    }
  }

  return LMV_RETURN_NO_ERROR;
}

MINT32 LMVDrvImp::Complement2(MUINT32 value, MUINT32 digit) {
  MINT32 Result = 0;
  if (((value >> (digit - 1)) & 0x1) == 1) {  // negative
    Result = 0 - (MINT32)((~value + 1) & ((1 << digit) - 1));
  } else {
    Result = (MINT32)(value & ((1 << digit) - 1));
  }
  return Result;
}

MVOID LMVDrvImp::BoundaryCheck(MUINT32* aInput,
                               const MUINT32& upBound,
                               const MUINT32& lowBound) {
  if (*aInput > upBound) {
    *aInput = upBound;
  }
  if (*aInput < lowBound) {
    *aInput = lowBound;
  }
}

MINT64 LMVDrvImp::GetTimeStamp(const MUINT32& aSec, const MUINT32& aUs) {
  return aSec * 1000000000LL + aUs * 1000LL;
}
