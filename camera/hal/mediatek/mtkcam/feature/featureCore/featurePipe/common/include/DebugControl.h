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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_DEBUGCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_DEBUGCONTROL_H_

// Properties
#define KEY_TUNING_BUF_POOL_PROTECT "vendor.debug.fpipe.tuning.protect"

#define VAL_TUNING_BUF_PROTECT \
  (-1)  // -1:not set, 0: force no use, 1: force use

// include files
#define TRACE_BUFFER_POOL 0
#define TRACE_CAM_GRAPH 0
#define TRACE_CAM_NODE 0
#define TRACE_CAM_PIPE 0
#define TRACE_CAM_THREAD_NODE 0
#define TRACE_COUNT_DOWN_LATCH 0
#define TRACE_WAIT_QUEUE 0

// src files
#define TRACE_CAM_THREAD 0
#define TRACE_DEBUG_UTIL 0
#define TRACE_FAT_IMAGE_BUFFER_POOL 0
#define TRACE_GRAPHIC_BUFFER_POOL 0
#define TRACE_IIBUFFER 0
#define TRACE_IMAGE_BUFFER_POOL 0
#define TRACE_TUNING_BUFFER_POOL 0
#define TRACE_NATIVEBUFFER_WRAPPER 0
#define TRACE_PROFILE 0
#define TRACE_SYNC_UTIL 0
#define TRACE_SEQ_UTIL 0
#define TRACE_IO_UTIL 0
#define TRACE_WAIT_HUB 0
#define TRACE_JPEG_ENCODE_THREAD 0

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_DEBUGCONTROL_H_

// only FeaturePipe/core files include this DebugControl.h
// so always redefine PIPE_MODULE_TAG back to FeaturePipe
#undef PIPE_MODULE_TAG
#define PIPE_MODULE_TAG "MtkCam/FeaturePipe"
