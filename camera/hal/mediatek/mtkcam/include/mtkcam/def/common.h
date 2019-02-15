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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_COMMON_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_COMMON_H_
/******************************************************************************
 *
 ******************************************************************************/
//
#include <string.h>
//
#include "Errors.h"
#include "BuiltinTypes.h"
#include "BasicTypes.h"
#include "UITypes.h"
#include "TypeManip.h"
//
#include "ImageFormat.h"
#include "Modes.h"

#ifndef __unused
#define __unused __attribute__((__unused__))
#endif

#define DUMP_PATH "/var/cache/camera/camera_dump"
#define CAMERA_PATH "/run/camera"
#define CAL_DUMP_PATH CAMERA_PATH "/EEPROM"
#define MVHDR_DUMP_PATH CAMERA_PATH "/MVHDR"
#define FD_DUMP_PATH CAMERA_PATH "/FD"
#define JPEG_DUMP_PATH CAMERA_PATH "/jpeg"
#define RAW16_DUMP_PATH CAMERA_PATH "/RAW16"
#define P1NODE_DUMP_PATH CAMERA_PATH "/P1NODE"
#define CAMERAPROP "/var/cache/camera/camera.prop"

#ifndef VISIBILITY_PUBLIC
#define VISIBILITY_PUBLIC __attribute__((visibility("default")))
#endif

//
/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_COMMON_H_
