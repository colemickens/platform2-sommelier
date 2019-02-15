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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_SENINF_DEFINE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_SENINF_DEFINE_H_

/*************************************************
 *
 **************************************************/
/* In KERNEL mode,SHOULD be sync with mediatype.h */
/* CHECK before remove or modify */
/* #undef BOOL */
/* #define BOOL signed int */
#ifndef _MEDIA_TYPES_H
typedef unsigned char MUINT8;
typedef uint16_t MUINT16;
typedef unsigned int MUINT32;
typedef signed char MINT8;
typedef int16_t MINT16;
typedef signed int MINT32;
#endif

struct KD_SENINF_MMAP {
  MUINT32 map_addr;
  MUINT32 map_length;
};

struct KD_SENINF_REG {
  struct KD_SENINF_MMAP seninf;
  struct KD_SENINF_MMAP ana;
  struct KD_SENINF_MMAP gpio;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_SENINF_DEFINE_H_
