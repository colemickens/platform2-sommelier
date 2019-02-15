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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_CAMERA_TYPEDEF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_CAMERA_TYPEDEF_H_

#include <linux/bug.h>

/* ------------------------*/
/* Basic Type Definitions */
/* -----------------------*/

typedef int32_t LONG;
typedef unsigned char UBYTE;
typedef int16_t SHORT;

typedef signed char kal_int8;
typedef int16_t kal_int16;
typedef signed int kal_int32;
typedef int64_t kal_int64;
typedef unsigned char kal_uint8;
typedef uint16_t kal_uint16;
typedef unsigned int kal_uint32;
typedef uint64_t kal_uint64;
typedef char kal_char;

typedef unsigned int* UINT32P;
typedef volatile uint16_t* UINT16P;
typedef volatile unsigned char* UINT8P;
typedef unsigned char* U8P;

typedef unsigned char U8;
typedef signed char S8;
typedef uint16_t U16;
typedef int16_t S16;
typedef unsigned int U32;
typedef signed int S32;
typedef uint64_t U64;
typedef int64_t S64;
/* typedef unsigned char       bool; */

typedef unsigned char UINT8;
typedef uint16_t UINT16;
typedef unsigned int UINT32;
typedef uint16_t USHORT;
typedef signed char INT8;
typedef int16_t INT16;
typedef signed int INT32;
typedef unsigned int DWORD;
typedef void VOID;
typedef unsigned char BYTE;
typedef float FLOAT;

typedef char* LPCSTR;
typedef int16_t* LPWSTR;

/* -----------*/
/* Constants */
/* ----------*/
#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (1)
#endif

#ifndef NULL
#define NULL (0)
#endif

/* enum boolean {false, true}; */
enum { RX, TX, NONE };

#ifndef BOOL
typedef unsigned char BOOL;
#endif

typedef enum {
  KAL_FALSE = 0,
  KAL_TRUE = 1,
} kal_bool;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_CAMERA_TYPEDEF_H_
