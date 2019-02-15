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
 * @file EIS_Type.h
 *
 * EIS Type Header File
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EIS_EIS_TYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EIS_EIS_TYPE_H_

#include <mtkcam/def/common.h>
#include <mtkcam/drv/IHalSensor.h>

#include <mtkcam/feature/eis/eis_ext.h>

/*********************************************************************
 * Define Value
 *********************************************************************/

#include <libeis/MTKEis.h>

#define EIS_FE_MAX_INPUT_W (2400)
#define EIS_FE_MAX_INPUT_H (1360)

#define ALGOPT_FHD_THR_W (2400)
#define ALGOPT_FHD_THR_H (1800)

#define LMVO_MEMORY_SIZE (256)  // 32 * 64 (bits) = 256 bytes

/*********************************************************************
 * ENUM
 *********************************************************************/

/**
 *@brief Return enum of EIS
 */
typedef enum {
  EIS_RETURN_NO_ERROR = 0,             //! The function work successfully
  EIS_RETURN_UNKNOWN_ERROR = 0x0001,   //! Unknown error
  EIS_RETURN_INVALID_DRIVER = 0x0002,  //! invalid driver object
  EIS_RETURN_API_FAIL = 0x0003,        //! api fail
  EIS_RETURN_INVALID_PARA = 0x0004,    //! invalid parameter
  EIS_RETURN_NULL_OBJ = 0x0005,        //! null object
  EIS_RETURN_MEMORY_ERROR = 0x0006,    //! memory error
  EIS_RETURN_EISO_MISS = 0x0007        //! EISO data missed
} EIS_ERROR_ENUM;

/*********************************************************************
 * Struct
 *********************************************************************/

/**
 * @brief EIS config structure
 *
 */
typedef struct EIS_HAL_CONFIG {
  MUINT32 imgiW;    // DMA IMGI input width,use in pass2 configure
  MUINT32 imgiH;    // DMA IMGI input height,use in pass2 configure
  MUINT32 crzOutW;  // use in pass2 configure
  MUINT32 crzOutH;  // use in pass2 configure
  MUINT32 srzOutW;  // use in pass2 configure
  MUINT32 srzOutH;  // use in pass2 configure
  MUINT32 rssoWidth;
  MUINT32 rssoHeight;
  MUINT32 feTargetW;   // use in pass2 configure
  MUINT32 feTargetH;   // use in pass2 configure
  MUINT32 gpuTargetW;  // use in pass2 configure
  MUINT32 gpuTargetH;  // use in pass2 configure
  MUINT32 cropX;       // MW crop setting of X,use in pass2 configure
  MUINT32 cropY;       // MW crop setting of Y,use in pass2 configure
  MINT32 gmv_X;
  MINT32 gmv_Y;
  MINT32 confX;
  MINT32 confY;
  MINT32 gmv_exist;
  MUINT32 sensorIdx;
  MUINT32 process_mode;
  MUINT32 process_idx;

  MINT32 sensor_Width;     // sensor
  MINT32 sensor_Height;    // sensor
  MINT32 rrz_crop_Width;   // sensor crop
  MINT32 rrz_crop_Height;  // sensor crop
  MINT32 rrz_crop_X;
  MINT32 rrz_crop_Y;
  MINT32 rrz_scale_Width;   // RRZ output
  MINT32 rrz_scale_Height;  // RRZ output
  MINT32 is_multiSensor;
  MINT32 fov_wide_idx;
  MINT32 fov_tele_idx;
  MINT32 fov_align_Width;   // fov aligned output width
  MINT32 fov_align_Height;  // fov aligned output height
  MINT32 warp_precision;

  MUINT32 vHDREnabled;
  MUINT32 lmvDataEnabled;
  EIS_STATISTIC_STRUCT* lmv_data;
  MINT32 recordNo;
  MINT64 timestamp;
  //
  EIS_HAL_CONFIG()
      : imgiW(0),
        imgiH(0),
        crzOutW(0),
        crzOutH(0),
        srzOutW(0),
        srzOutH(0),
        rssoWidth(0),
        rssoHeight(0),
        feTargetW(0),
        feTargetH(0),
        gpuTargetW(0),
        gpuTargetH(0),
        cropX(0),
        cropY(0),
        gmv_X(0),
        gmv_Y(0),
        confX(0),
        confY(0),
        gmv_exist(0),
        sensorIdx(0),
        process_mode(0),
        process_idx(0),
        sensor_Width(0),
        sensor_Height(0),
        rrz_crop_Width(0),
        rrz_crop_Height(0),
        rrz_crop_X(0),
        rrz_crop_Y(0),
        rrz_scale_Width(0),
        rrz_scale_Height(0),
        is_multiSensor(0),
        fov_wide_idx(0),
        fov_tele_idx(0),
        fov_align_Width(0),
        fov_align_Height(0),
        warp_precision(5),
        vHDREnabled(0),
        lmvDataEnabled(0),
        lmv_data(NULL),
        recordNo(0),
        timestamp(0) {}
} EIS_HAL_CONFIG_DATA;

typedef struct LMV_PACKAGE_T {
  MUINT32 enabled;
  EIS_STATISTIC_STRUCT data;

  LMV_PACKAGE_T() : enabled(0) {
    memset(&data, 0, sizeof(EIS_STATISTIC_STRUCT));
  }
} LMV_PACKAGE;

/**
 *@brief FEO register info
 */
#define MULTISCALE_FEFM (3)

typedef struct MULTISCALE_INFO_T {
  MINT32 MultiScale_width[MULTISCALE_FEFM];
  MINT32 MultiScale_height[MULTISCALE_FEFM];
  MINT32 MultiScale_blocksize[MULTISCALE_FEFM];
} MULTISCALE_INFO;

typedef struct FEFM_PACKAGE_T {
  MUINT16* FE[MULTISCALE_FEFM];
  MUINT16* LastFE[MULTISCALE_FEFM];

  MUINT16* ForwardFM[MULTISCALE_FEFM];
  MUINT16* BackwardFM[MULTISCALE_FEFM];
  MUINT32 ForwardFMREG[MULTISCALE_FEFM];
  MUINT32 BackwardFMREG[MULTISCALE_FEFM];

  FEFM_PACKAGE_T(MUINT32 _FMRegValue = 0) {
    MUINT32 i;
    for (i = 0; i < MULTISCALE_FEFM; i++) {
      FE[i] = 0;
      LastFE[i] = 0;
      ForwardFM[i] = 0;
      BackwardFM[i] = 0;
      ForwardFMREG[i] = _FMRegValue;
      BackwardFMREG[i] = _FMRegValue;
    }
  }
} FEFM_PACKAGE;

typedef struct RSCME_PACKAGE_T {
  MUINT8* RSCME_mv;
  MUINT8* RSCME_var;

  RSCME_PACKAGE_T() : RSCME_mv(0), RSCME_var(0) {}
} RSCME_PACKAGE;

typedef struct FSC_INFO_T {
  MINT32 numSlices;
  MBOOL isEnabled;
  FSC_INFO_T() : numSlices(0), isEnabled(MFALSE) {}
} FSC_INFO;

typedef struct FSC_PACKAGE_T {
  MINT32 procWidth;
  MINT32 procHeight;
  MINT32* scalingFactor;

  FSC_PACKAGE_T() : procWidth(0), procHeight(0), scalingFactor(NULL) {}
} FSC_PACKAGE;

typedef struct IMAGE_BASED_DATA_T {
  LMV_PACKAGE* lmvData;
  FEFM_PACKAGE* fefmData;
  RSCME_PACKAGE* rscData;
  FSC_PACKAGE* fscData;

  IMAGE_BASED_DATA_T()
      : lmvData(NULL), fefmData(NULL), rscData(NULL), fscData(NULL) {}
} IMAGE_BASED_DATA;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_EIS_EIS_TYPE_H_
