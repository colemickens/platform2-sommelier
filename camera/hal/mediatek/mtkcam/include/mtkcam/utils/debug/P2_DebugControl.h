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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_DEBUG_P2_DEBUGCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_DEBUG_P2_DEBUGCONTROL_H_

#define REPLACE_HAL1_P2F 1
#define USE_BASIC_PROCESSOR 0
#define USE_CAPTURE_PROCESSOR 0

#define P2_POLICY_DYNAMIC 0
#define P2_POLICY_FORCE_BASIC 1
#define P2_POLICY_FORCE_STREAMING 2

#define KEY_P2_PROC_POLICY "vendor.debug.mtkcam.p2.processor"
#define KEY_P2_LOG "vendor.debug.mtkcam.p2.log"
#define KEY_P2_REDIRECT "vendor.debug.mtkcam.p2.redirect"
#define KEY_TRACE_P2 "trace.p2"

#define VAL_P2_PROC_DFT_POLICY P2_POLICY_DYNAMIC

#define VAL_P2_DUMP 0
#define VAL_P2_DUMP_COUNT 1
#define VAL_P2_LOG 0
#define VAL_P2_REDIRECT 0

#define USE_CLASS_TRACE 1

#define TRACE_BASIC_PROCESSOR 0
#define TRACE_BASIC_HIGH_SPEED 0
#define TRACE_CAPTURE_PROCESSOR 0
#define TRACE_CROPPER 0
#define TRACE_DISPATCH_PROCESSOR 0
#define TRACE_LMV_INFO 0
#define TRACE_MW_FRAME 0
#define TRACE_MW_FRAME_REQUEST 0
#define TRACE_MW_IMG 0
#define TRACE_MW_INFO 0
#define TRACE_MW_META 0
#define TRACE_P2_DISPATCH_REQUEST 0
#define TRACE_P2_SENSOR_REQUEST 0
#define TRACE_P2_DUMP_PLUGIN 0
#define TRACE_P2_FRAME_HOLDER 0
#define TRACE_P2_FRAME_REQUEST 0
#define TRACE_P2_IMG 0
#define TRACE_P2_INFO 0
#define TRACE_P2_LOGGER 0
#define TRACE_P2_NODE 0
#define TRACE_P2_REQUEST 0
#define TRACE_P2_SCANLINE_PLUGIN 0
#define TRACE_P2_DRAWID_PLUGIN 0
#define TRACE_P2_META 0
#define TRACE_P2_UTIL 0
#define TRACE_PROCESSOR 0
#define TRACE_STREAMING_PROCESSOR 0
#define TRACE_STREAMING_3DNR 0
#define TRACE_STREAMING_EIS 0
#define TRACE_STREAMING_FSC 0

#define ATRACE_ENABLED() (0)

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_DEBUG_P2_DEBUGCONTROL_H_

#undef P2_MODULE_TAG
#define P2_MODULE_TAG "MtkCam/P2"
#undef P2_CLASS_TAG
#undef P2_TRACE
