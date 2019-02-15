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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_DEBUGCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_DEBUGCONTROL_H_

#define KEY_FORCE_EIS "vendor.debug.fpipe.force.eis"
#define KEY_FORCE_EIS_25 "vendor.debug.fpipe.force.eis25"
#define KEY_FORCE_EIS_30 "vendor.debug.fpipe.force.eis30"
#define KEY_FORCE_EIS_QUEUE "vendor.debug.fpipe.force.eisq"
#define KEY_FORCE_3DNR "vendor.debug.fpipe.force.3dnr"
#define KEY_FORCE_VHDR "vendor.debug.fpipe.force.vhdr"
#define KEY_FORCE_WARP_PASS "vendor.debug.fpipe.force.warp.pass"
#define KEY_FORCE_GPU_OUT "vendor.debug.fpipe.force.gpu.out"
#define KEY_FORCE_GPU_RGBA "vendor.debug.fpipe.force.gpu.rgba"
#define KEY_FORCE_IMG3O "vendor.debug.fpipe.force.img3o"
#define KEY_FORCE_BUF "vendor.debug.fpipe.force.buf"
#define KEY_FORCE_VENDOR_V1 "vendor.debug.fpipe.force.vendor.v1"
#define KEY_FORCE_VENDOR_V2 "vendor.debug.fpipe.force.vendor.v2"
#define KEY_FORCE_DUMMY "vendor.debug.fpipe.force.dummy"
#define KEY_ENABLE_VENDOR_V1 "vendor.debug.fpipe.enable.vendor.v1"
#define KEY_ENABLE_VENDOR_V1_SIZE "vendor.debug.fpipe.vendor.v1.size"
#define KEY_ENABLE_VENDOR_V1_FORMAT "vendor.debug.fpipe.vendor.v1.format"
#define KEY_ENABLE_DUMMY "vendor.debug.fpipe.enable.dummy"
#define KEY_ENABLE_PURE_YUV "vendor.debug.fpipe.enable.pureyuv"
#define KEY_USE_PER_FRAME_SETTING "vendor.debug.fpipe.frame.setting"
#define KEY_DEBUG_DUMP "vendor.debug.fpipe.force.dump"
#define KEY_DEBUG_DUMP_COUNT "vendor.debug.fpipe.force.dump.count"
#define KEY_DEBUG_DUMP_BY_RECORDNO "vendor.debug.fpipe.dump.by.recordno"
#define KEY_FORCE_RSC_TUNING "vendor.debug.fpipe.force.rsc.tuning"
#define KEY_FORCE_PRINT_IO "vendor.debug.fpipe.force.printio"

#define KEY_DEBUG_TPI "vendor.debug.tpi.s"
#define KEY_DEBUG_TPI_LOG "vendor.debug.tpi.s.log"
#define KEY_DEBUG_TPI_DUMP "vendor.debug.tpi.s.dump"
#define KEY_DEBUG_TPI_SCAN "vendor.debug.tpi.s.scan"
#define KEY_DEBUG_TPI_BYPASS "vendor.debug.tpi.s.bypass"

#define STR_ALLOCATE "pool allocate:"

#define MAX_TPI_COUNT ((MUINT32)3)

#define USE_YUY2_FULL_IMG 1
#define USE_WPE_STAND_ALONE 0

#define SUPPORT_3A_HAL 1
#define SUPPORT_GPU_YV12 1
#define SUPPORT_GPU_CROP 1
#define SUPPORT_VENDOR_NODE 0
#define SUPPORT_DUMMY_NODE 0
#define SUPPORT_PURE_YUV 0
#define SUPPORT_FAKE_EIS_30 0
#define SUPPORT_FAKE_WPE 0
#if (MTKCAM_FOV_USE_WPE == 1)
#define SUPPORT_WPE 1
#else
#define SUPPORT_WPE 0
#endif

#define SUPPORT_P2AMDP_THREAD_MERGE 0

#define SUPPORT_VENDOR_SIZE 0
#define SUPPORT_VENDOR_FORMAT 0
#define SUPPORT_VENDOR_FULL_FORMAT eImgFmt_NV21

#define SUPPORT_FOV (MTKCAM_HAVE_DUAL_ZOOM_SUPPORT == 1)

#define DEBUG_TIMER 1

#define DEV_VFB_READY 0
#define DEV_P2B_READY 0

#define NO_FORCE 0
#define FORCE_ON 1
#define FORCE_OFF 2

#define VAL_FORCE_EIS 0
#define VAL_FORCE_EIS_25 0
#define VAL_FORCE_EIS_30 0
#define VAL_FORCE_EIS_QUEUE 0
#define VAL_FORCE_3DNR 0
#define VAL_FORCE_VHDR 0
#define VAL_FORCE_WARP_PASS 0
#define VAL_FORCE_GPU_OUT 0
#define VAL_FORCE_GPU_RGBA (!SUPPORT_GPU_YV12)
#define VAL_FORCE_IMG3O 0
#define VAL_FORCE_BUF 0
#define VAL_FORCE_VENDOR_V1 0
#define VAL_FORCE_VENDOR_V2 0
#define VAL_FORCE_DUMMY 0
#define VAL_DEBUG_DUMP 0
#define VAL_DEBUG_DUMP_COUNT 1
#define VAL_DEBUG_DUMP_BY_RECORDNO 0
#define VAL_USE_PER_FRAME_SETTING 0
#define VAL_FORCE_RSC_TUNING 0
#define VAL_FORCE_PRINT_IO 0

#define TRACE_STREAMING_FEATURE_COMMON 0
#define TRACE_STREAMING_FEATURE_DATA 0
#define TRACE_STREAMING_FEATURE_NODE 0
#define TRACE_STREAMING_FEATURE_PIPE 0
#define TRACE_STREAMING_FEATURE_USAGE 0
#define TRACE_STREAMING_FEATURE_TIMER 0
#define TRACE_IMG_BUFFER_STORE 0
#define TRACE_ROOT_NODE 0
#define TRACE_P2_CAM_CONTEXT 0
#define TRACE_P2A_NODE 0
#define TRACE_P2A_3DNR 0
#define TRACE_P2A_VHDR 0
#define TRACE_FM_HAL 0
#define TRACE_EIS_NODE 0
#define TRACE_GPU_NODE 0
#define TRACE_VENDOR_NODE 0
#define TRACE_VMDP_NODE 0
#define TRACE_DUMMY_NODE 0
#define TRACE_NULL_NODE 0
#define TRACE_RSC_NODE 0
#define TRACE_WARP_NODE 0
#define TRACE_HELPER_NODE 0
#define TRACE_QPARAMS_BASE 0
#define TRACE_WARP_BASE 0
#define TRACE_GPU_WARP 0
#define TRACE_WPE_WARP 0
#define TRACE_COOKIE_STORE 0
#define TRACE_NORMAL_STREAM_BASE 0
#define TRACE_RSC_STREAM_BASE 0
#define TRACE_RSC_TUNING_STREAM 0
#define TRACE_WARP_STREAM_BASE 0
#define TRACE_GPU_WARP_STREAM_BASE 0
#define TRACE_WPE_WARP_STREAM_BASE 0
#define TRACE_MDP_WRAPPER 0
// dual zoom
#define TRACE_FOV_NODE 0
#define TRACE_FOVWARP_NODE 0
#define TRACE_FOV_HAL 0
#define TRACE_P2A_FOV 0
// dual preview mode
#define TRACE_N3D_NODE 0
#define TRACE_N3DP2_NODE 0
#define TRACE_EIS_Q_CONTROL 0
#define TRACE_TPI_NODE 0
#define TRACE_TPI_USAGE 0

namespace NSCam {
namespace NSCamFeature {
namespace NSFeaturePipe {}  // namespace NSFeaturePipe
}  // namespace NSCamFeature
}  // namespace NSCam

#ifdef PIPE_MODULE_TAG
#undef PIPE_MODULE_TAG
#endif
#define PIPE_MODULE_TAG "MtkCam/StreamingPipe"
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_STREAMING_DEBUGCONTROL_H_
