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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_ILOGHEADER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_ILOGHEADER_H_

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#undef __Q
#undef _Q
#define __Q(v) #v
#define _Q(v) __Q(v)

#ifndef ILOG_MODULE_TAG
#error "Must define ILOG_MODULE_TAG before include ILogHeader.h"
#endif

#define ILOG_CLASS_TAG_MUST 0

#ifdef ILOG_CLASS_TAG
#define LOG_TAG _Q(ILOG_MODULE_TAG) "/" _Q(ILOG_CLASS_TAG)
#elif !(ILOG_CLASS_TAG_MUST)
#define LOG_TAG _Q(ILOG_MODULE_TAG)
#else
#error "Must define ILOG_CLASS_TAG before include ILogHeader.h"
#endif

#include <mtkcam/utils/std/Log.h>
#include <string>

#ifndef ILOG_TRACE
#define ILOG_TRACE 0
#endif

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_STD_ILOGHEADER_H_
