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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_LOGHEADER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_LOGHEADER_H_
#include "P2_Logger.h"

#ifndef P2_MODULE_TAG
#error "Must define P2_MODULE_TAG before include P2_Log.h"
#endif
#ifndef P2_CLASS_TAG
#error "Must define P2_CLASS_TAG before include P2_Log.h"
#endif
#ifndef P2_TRACE
#define P2_TRACE 0
#endif

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_HWNODE_P2_P2_LOGHEADER_H_

#undef __Q
#undef _Q
#define __Q(v) #v
#define _Q(v) __Q(v)
#undef LOG_TAG
#define LOG_TAG P2_MODULE_TAG "/" _Q(P2_CLASS_TAG)
#include <mtkcam/utils/std/Log.h>
#include <string>
