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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_AAA_DBG_AAA_PARAM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_AAA_DBG_AAA_PARAM_H_

#include "aaa/dbg_aaa_common_param.h"

#define MAXIMUM_AAA_DEBUG_COMM_SIZE 32
typedef union {
  struct {
    MUINT32 chkSum;
    MUINT32 ver;
  };
  MUINT8 Data[MAXIMUM_AAA_DEBUG_COMM_SIZE];
} AAA_DEBUG_COMM_T;
static_assert(sizeof(AAA_DEBUG_COMM_T) == MAXIMUM_AAA_DEBUG_COMM_SIZE,
              "AAA_DEBUG_COM_T size mismatch");

#include "aaa/dbg_ae_param.h"
#include "aaa/dbg_af_param.h"
#include "aaa/dbg_awb_param.h"
#include "aaa/dbg_flash_param.h"
#include "aaa/dbg_flicker_param.h"
#include "aaa/dbg_isp_param.h"
#include "aaa/dbg_shading_param.h"
//
#define AAA_DEBUG_AE_MODULE_ID 0x6001
#define AAA_DEBUG_AF_MODULE_ID 0x6002
#define AAA_DEBUG_AWB_MODULE_ID 0x6003
#define AAA_DEBUG_FLASH_MODULE_ID 0x6004
#define AAA_DEBUG_FLICKER_MODULE_ID 0x6005
#define AAA_DEBUG_SHADING_MODULE_ID 0x6006
#define AAA_DEBUG_AWB_DATA_MODULE_ID 0x6007
#define AAA_DEBUG_AE_PLINE_MODULE_ID 0x6008
#define AAA_DEBUG_SHADTBL2_MODULE_ID 0x6009
//

typedef struct {
  MUINT32 u4Size;
  AAA_DEBUG_COMM_T rAE;
  AAA_DEBUG_COMM_T rAF;
  AAA_DEBUG_COMM_T rFLASH;
  AAA_DEBUG_COMM_T rFLICKER;
  AAA_DEBUG_COMM_T rSHADING;
} COMMON_DEBUG_INFO1_T;

typedef struct {
  MUINT32 u4Size;
  AAA_DEBUG_COMM_T rAWB;
  AAA_DEBUG_COMM_T rISP;
} COMMON_DEBUG_INFO2_T;

typedef struct {
  struct Header {
    MUINT32 u4KeyID;
    MUINT32 u4ModuleCount;
    MUINT32 u4AEDebugInfoOffset;
    MUINT32 u4AFDebugInfoOffset;
    MUINT32 u4FlashDebugInfoOffset;
    MUINT32 u4FlickerDebugInfoOffset;
    MUINT32 u4ShadingDebugInfoOffset;
    COMMON_DEBUG_INFO1_T rCommDebugInfo;
  } hdr;

  // TAG
  AE_DEBUG_INFO_T rAEDebugInfo;
  AF_DEBUG_INFO_T rAFDebugInfo;
  FLASH_DEBUG_INFO_T rFlashDebugInfo;
  FLICKER_DEBUG_INFO_T rFlickerDebugInfo;
  SHADING_DEBUG_INFO_T rShadigDebugInfo;
} AAA_DEBUG_INFO1_T;

typedef struct {
  struct Header {
    MUINT32 u4KeyID;
    MUINT32 u4ModuleCount;
    MUINT32 u4AWBDebugInfoOffset;
    MUINT32 u4ISPDebugInfoOffset;
    MUINT32 u4ISPP1RegDataOffset;
    MUINT32 u4ISPP2RegDataOffset;
    MUINT32 u4MFBRegInfoOffset;
    MUINT32 u4AWBDebugDataOffset;
    COMMON_DEBUG_INFO2_T rCommDebugInfo;
  } hdr;

  // TAG
  AWB_DEBUG_INFO_T rAWBDebugInfo;

  // None TAG
  // ISP Tag, P1 Table, P2 Table, MFB Table
  NSIspExifDebug::IspExifDebugInfo_T rISPDebugInfo;
  // AWB debug Table (Struct)
  AWB_DEBUG_DATA_T rAWBDebugData;
} AAA_DEBUG_INFO2_T;

#define DEFAULT_DATA (0xFF7C)
static_assert(sizeof(AAA_DEBUG_INFO1_T) <= DEFAULT_DATA,
              "Debug Info exceed EXIF limitation, please discuss with EXIF and "
              "Debug Parser owner for solution!");
static_assert(sizeof(AAA_DEBUG_INFO2_T) <= DEFAULT_DATA,
              "Debug Info exceed EXIF limitation, please discuss with EXIF and "
              "Debug Parser owner for solution!");

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_AAA_DBG_AAA_PARAM_H_
