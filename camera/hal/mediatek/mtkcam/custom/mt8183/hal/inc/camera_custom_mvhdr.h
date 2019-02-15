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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_MVHDR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_MVHDR_H_

#include "inc/camera_custom_ivhdr.h"
#include "inc/camera_custom_types.h"  // For MUINT*/MINT*/MVOID/MBOOL type definitions.

#define CUST_MVHDR_DEBUG \
  0  // Enable this will dump HDR Debug Information into SDCARD
#define MVHDR_DEBUG_OUTPUT_FOLDER MVHDR_DUMP_PATH  // For ALPS.JB.
/**************************************************************************
 *                      D E F I N E S / M A C R O S                       *
 **************************************************************************/

/**************************************************************************
 *     E N U M / S T R U C T / T Y P E D E F    D E C L A R A T I O N     *
 **************************************************************************/

/**************************************************************************
 *                 E X T E R N A L    R E F E R E N C E S                 *
 **************************************************************************/

/**************************************************************************
 *        P U B L I C    F U N C T I O N    D E C L A R A T I O N         *
 **************************************************************************/

/*******************************************************************************
 * IVHDR exposure setting
 ******************************************************************************/
typedef struct mVHDRInputParam_S {
  MUINT32 u4SensorID;   //
  MUINT32 u4OBValue;    // 10 bits
  MUINT32 u4ISPGain;    // 1x=1024
  MUINT16 u2StatWidth;  // statistic width
  MUINT16 u2StatHight;  // statistic height
  MUINT16 u2ShutterRatio;
  MVOID* pDataPointer;  //
} mVHDRInputParam_T;

typedef struct mVHDROutputParam_S {
  MBOOL bUpdateSensorAWB;  // MTRUE : update, MFALSE : don't update
  MVOID* pDataPointer;     //
} mVHDROutputParam_T;

typedef struct mVHDR_SWHDR_InputParam_S {
  MINT32 i4Ratio;
  MINT32 LEMax;
  MINT32 SEMax;
} mVHDR_SWHDR_InputParam_T;

typedef struct mVHDR_SWHDR_OutputParam_S {
  MINT32 i4SEDeltaEVx100;
} mVHDR_SWHDR_OutputParam_T;

typedef struct mVHDR_TRANSFER_Param_S {
  MBOOL bIs60HZ;  // Flicker 50Hz:0, 60Hz:1
  MBOOL bSEInput;
  MINT32 i4LV;
  MUINT16 u2SelectMode;
} mVHDR_TRANSFER_Param_T;

MVOID decodemVHDRStatistic(const mVHDRInputParam_T& rInput,
                           mVHDROutputParam_T* rOutput);
MVOID getMVHDR_AEInfo(const mVHDR_SWHDR_InputParam_T& rInput,
                      mVHDR_SWHDR_OutputParam_T* rOutput);
MVOID getmVHDRExpSetting(mVHDR_TRANSFER_Param_T* rInputParam,
                         IVHDRExpSettingOutputParam_T* rOutput);
MBOOL isSESetting();

/**************************************************************************
 *                   C L A S S    D E C L A R A T I O N                   *
 **************************************************************************/

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_MVHDR_H_
