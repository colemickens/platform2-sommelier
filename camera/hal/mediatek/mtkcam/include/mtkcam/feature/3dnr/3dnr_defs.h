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
 * @file 3dnr_defs.h
 *
 * 3DNR Defs Header File
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_3DNR_3DNR_DEFS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_3DNR_3DNR_DEFS_H_

#include <mtkcam/def/BuiltinTypes.h>

namespace NSCam {
namespace NR3D {

/*********************************************************************
 * Define Value
 *********************************************************************/

static const char* LOG_LEVEL_PROPERTY = "vendor.camera.3dnr.log.level";
static const char* DEBUG_LEVEL_PROPERTY = "debug.camera.3dnr.level";
static const char* DEBUG_RESET_GMV_PROPERTY = "debug.camera.3dnr.reset_gmv";
#define NR3D_LMV_MAX_GMV_DEFAULT 32

/*********************************************************************
 * ENUM
 *********************************************************************/

/**
 *@brief Return enum of 3DNR Modr for P2 flow.
 */
enum E3DNR_MODE_MASK {
  E3DNR_MODE_MASK_HAL_FORCE_SUPPORT = 1 << 0,  // hal force support 3DNR
  E3DNR_MODE_MASK_UI_SUPPORT = 1 << 1,         // feature option on/off
  E3DNR_MODE_MASK_RSC_EN = 1 << 5,             // enable RSC support for 3DNR
  E3DNR_MODE_MASK_SL2E_EN = 1 << 6,            // enable SL2E
};

#define E3DNR_MODE_MASK_ENABLED(x, mask) ((x) & (mask))

/*********************************************************************
 * Struct
 *********************************************************************/

struct GyroData {
  MBOOL isValid;
  MFLOAT x, y, z;

  GyroData() : isValid(MFALSE), x(0.0f), y(0.0f), z(0.0f) {}
};

/******************************************************************************
 *
 * @struct NR3DMVInfo
 * @brief parameter for set nr3d
 * @details
 *
 ******************************************************************************/

struct NR3DMVInfo {
  enum Status { INVALID = 0, VALID };

  // 3dnr vipi: needs x_int/y_int/gmvX/gmvY
  // ISP smoothNR3D: needs gmvX/gmvY/confX/confY/maxGMV
  MINT32 status;
  MUINT32 x_int;
  MUINT32 y_int;
  MINT32 gmvX;
  MINT32 gmvY;
  MINT32 confX;
  MINT32 confY;
  MINT32 maxGMV;

  NR3DMVInfo()
      : status(INVALID),
        x_int(0),
        y_int(0),
        gmvX(0),
        gmvY(0),
        confX(0),
        confY(0),
        maxGMV(NR3D_LMV_MAX_GMV_DEFAULT) {}
};

struct NR3DTuningInfo {
  MBOOL canEnable3dnrOnFrame;
  MINT32 isoThreshold;
  NR3DMVInfo mvInfo;
  MSize inputSize;  // of NR3D hardware
  MRect inputCrop;
};

};      // namespace NR3D
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_3DNR_3DNR_DEFS_H_
