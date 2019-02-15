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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_MEDIATYPES_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_MEDIATYPES_H_

/*******************************************************************************
 *
 *   !!! Remove Me If Possible
 *   !!! Do Not Include Me If Possible
 *   !!! Do Not Modify Me If Possbile
 *   !!! Make sure the compatibility with the original one.
 *   !!! Do Not Rename "_MEDIA_TYPES_H" Unless No Side Effect.
 *
 ******************************************************************************/

/*******************************************************************************
 *
 ******************************************************************************/
//
typedef unsigned char u8;
typedef uint16_t u16;
typedef unsigned int u32;
//
typedef void MHAL_VOID;
typedef char MHAL_BOOL;
typedef char MHAL_CHAR;
typedef signed char MHAL_INT8;
typedef int16_t MHAL_INT16;
typedef signed int MHAL_INT32;
typedef unsigned char MHAL_UCHAR;
typedef unsigned char MHAL_UINT8;
typedef uint16_t MHAL_UINT16;
typedef unsigned int MHAL_UINT32;
//
typedef MHAL_VOID MVOID;
typedef MHAL_UINT8 MUINT8;
typedef MHAL_UINT16 MUINT16;
typedef MHAL_UINT32 MUINT32;
typedef MHAL_INT32 MINT32;

/*******************************************************************************
 *
 ******************************************************************************/
#define READ32(addr) *reinterpret_cast<MUINT32*>(addr)
#define WRITE32(addr, val) *reinterpret_cast<MUINT32*>((addr) = (val)

#define MHAL_TRUE 1
#define MHAL_FALSE 0

/*******************************************************************************
 *
 ******************************************************************************/
// typedef signed char         CHAR;
// typedef char                UCHAR;
#define CHAR signed char
#define UCHAR char
typedef signed char INT8;
typedef unsigned char UINT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
// typedef signed int          BOOL;
#define BOOL signed int
// typedef signed int          INT32;
#define INT32 signed int
typedef unsigned int UINT32;
typedef int64_t INT64;
typedef uint64_t UINT64;
typedef float FLOAT;
typedef double DOUBLE;
typedef void VOID;

typedef INT32 MRESULT;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef NULL
#define NULL 0
#endif

/*******************************************************************************
 *
 ******************************************************************************/

/**
 * @par Enumeration
 *   MHAL_ERROR_ENUM
 * @par Description
 *   This is the return status of each MHAL function
 */
typedef enum {
  MHAL_NO_ERROR = 0,       ///< The function work successfully
  MHAL_INVALID_DRIVER,     ///< Error due to invalid driver
  MHAL_INVALID_CTRL_CODE,  ///< Error due to invalid control code
  MHAL_INVALID_PARA,       ///< Error due to invalid parameter
  MHAL_INVALID_MEMORY,     ///< Error due to invalid memory
  MHAL_INVALID_FORMAT,     ///< Error due to invalid file format
  MHAL_INVALID_RESOURCE,   ///< Error due to invalid resource, like IDP

  MHAL_UNKNOWN_ERROR = 0x80000000,  ///< Unknown error
  MHAL_ALL = 0xFFFFFFFF
} MHAL_ERROR_ENUM;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_MEDIATYPES_H_
