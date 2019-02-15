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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_BUILTINTYPES_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_BUILTINTYPES_H_

/******************************************************************************
 *
 ******************************************************************************/
#include <stdint.h>

/******************************************************************************
 *
 ******************************************************************************/
//
typedef signed char MINT8;
typedef unsigned char MUINT8;
//
typedef int16_t MINT16;
typedef uint16_t MUINT16;
//
typedef signed int MINT32;
typedef unsigned int MUINT32;
//
typedef int64_t MINT64;
typedef uint64_t MUINT64;
//
typedef signed int MINT;
typedef unsigned int MUINT;
//
typedef intptr_t MINTPTR;
typedef uintptr_t MUINTPTR;

/******************************************************************************
 *
 ******************************************************************************/
//
typedef void MVOID;
typedef float MFLOAT;
typedef double MDOUBLE;
//
typedef int MBOOL;
//
#ifndef MTRUE
#define MTRUE 1
#endif
#ifndef MFALSE
#define MFALSE 0
#endif
#ifndef MNULL
#define MNULL 0
#endif

typedef enum { KAL_FALSE, KAL_TRUE } kal_bool;

/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_BUILTINTYPES_H_
