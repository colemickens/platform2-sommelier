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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_IPOSTPROCFEFM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_IPOSTPROCFEFM_H_

#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSIoPipe {

/******************************************************************************
 * @struct FEInfo
 *
 * @brief FEInfo.
 *
 * @param[in] CmdIdx: specific command index: EPIPE_FE_INFO_CMD
 *
 * @param[in] moduleStruct: specific structure: FEInfo
 *
 * @param[in] s: size (i.e. width and height) in pixels.
 ******************************************************************************/
struct FEInfo {
  MUINT32 mFEDSCR_SBIT;
  MUINT32 mFETH_C;
  MUINT32 mFETH_G;
  MUINT32 mFEFLT_EN;
  MUINT32 mFEPARAM;
  MUINT32 mFEMODE;
  MUINT32 mFEYIDX;
  MUINT32 mFEXIDX;
  MUINT32 mFESTART_X;
  MUINT32 mFESTART_Y;
  MUINT32 mFEIN_HT;
  MUINT32 mFEIN_WD;
  FEInfo()
      : mFEDSCR_SBIT(0),
        mFETH_C(0),
        mFETH_G(0),
        mFEFLT_EN(0),
        mFEPARAM(0),
        mFEMODE(0),
        mFEYIDX(0),
        mFEXIDX(0),
        mFESTART_X(0),
        mFESTART_Y(0),
        mFEIN_HT(0),
        mFEIN_WD(0) {}
};

/******************************************************************************
 * @struct FMInfo
 *
 * @brief FMInfo.
 *
 * @param[in] CmdIdx: specific command index: EPIPE_FM_INFO_CMD
 *
 * @param[in] moduleStruct: specific structure: FMInfo
 *
 * @param[in] s: size (i.e. width and height) in pixels.
 ******************************************************************************/
struct FMInfo {
  MUINT32 mFMHEIGHT;
  MUINT32 mFMWIDTH;
  MUINT32 mFMSR_TYPE;
  MUINT32 mFMOFFSET_X;
  MUINT32 mFMOFFSET_Y;
  MUINT32 mFMRES_TH;
  MUINT32 mFMSAD_TH;
  MUINT32 mFMMIN_RATIO;
  FMInfo()
      : mFMHEIGHT(0),
        mFMWIDTH(0),
        mFMSR_TYPE(0),
        mFMOFFSET_X(0),
        mFMOFFSET_Y(0),
        mFMRES_TH(0),
        mFMSAD_TH(0),
        mFMMIN_RATIO(0) {}
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSIoPipe
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DRV_DEF_IPOSTPROCFEFM_H_
