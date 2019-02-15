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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_UTILS_COMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_UTILS_COMMON_H_

#include <mtkcam/def/common.h>
#include <mtkcam/utils/std/Log.h>

///////////////////////////////////////////////////////////////////////////////
//!  Error code formmat is:
//!
//!  Bit 31~24 is global, each module must follow it, bit 23~0 is defined by
//!  module | 31(1 bit) |30-24(7 bits) |         23-0   (24 bits)      | |
//!  Indicator | Module ID    |   Module-defined error Code   |
//!
//!  Example 1:
//!  | 31(1 bit) |30-24(7 bits) |   23-16(8 bits)   | 15-0(16 bits) |
//!  | Indicator | Module ID    | group or sub-mod  |    Err Code   |
//!
//!  Example 2:
//!  | 31(1 bit) |30-24(7 bits) | 23-12(12 bits)| 11-8(8 bits) | 7-0(16 bits)  |
//!  | Indicator | Module ID    |   line number |    group     |    Err Code   |
//!
//!  Indicator  : 0 - success, 1 - error
//!  Module ID  : module ID, defined below
//!  Extended   : module dependent, but providee macro to add partial line info
//!  Err code   : defined in each module's public include file,
//!               IF module ID is MODULE_COMMON, the errocode is
//!               defined here
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
//! Error code type definition
///////////////////////////////////////////////////////////////////////////
typedef MINT32 MRESULT;

///////////////////////////////////////////////////////////////////////////
//! Helper macros to define error code
///////////////////////////////////////////////////////////////////////////
#define ERRCODE(modid, errid)                                         \
  ((MINT32)((MUINT32)(0x80000000) | (MUINT32)((modid & 0x7f) << 24) | \
            (MUINT32)(errid & 0xffff)))

#define OKCODE(modid, okid)                                           \
  ((MINT32)((MUINT32)(0x00000000) | (MUINT32)((modid & 0x7f) << 24) | \
            (MUINT32)(okid & 0xffff)))

///////////////////////////////////////////////////////////////////////////
//! Helper macros to check error code
///////////////////////////////////////////////////////////////////////////
#define SUCCEEDED(Status) ((MRESULT)(Status) >= 0)
#define FAILED(Status) ((MRESULT)(Status) < 0)

typedef unsigned int MUINT32;
typedef uint16_t MUINT16;
typedef unsigned char MUINT8;

typedef signed int MINT32;
typedef int16_t MINT16;
typedef signed char MINT8;

typedef signed int MBOOL;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef NULL
#define NULL 0
#endif

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_COMMON_UTILS_COMMON_H_
