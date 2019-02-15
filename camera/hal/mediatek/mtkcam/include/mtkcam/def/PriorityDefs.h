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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_PRIORITYDEFS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_PRIORITYDEFS_H_
/******************************************************************************
 *  Priority Definitions.
 ******************************************************************************/

/******************************************************************************
 *  Nice value (SCHED_OTHER)
 ******************************************************************************/
#define SCHED_PRIORITY_FOREGROUND (-2)
#define SCHED_PRIORITY_NORMAL (0)
enum {
  //
  NICE_CAMERA_PASS1 = SCHED_PRIORITY_FOREGROUND,
  NICE_CAMERA_PASS2 = SCHED_PRIORITY_FOREGROUND,
  NICE_CAMERA_VSS_PASS2 = SCHED_PRIORITY_NORMAL,
  NICE_CAMERA_SM_PASS2 = SCHED_PRIORITY_NORMAL - 8,
  NICE_CAMERA_JPEG = SCHED_PRIORITY_NORMAL,
  NICE_CAMERA_SHOTCB = SCHED_PRIORITY_NORMAL,
  //
  NICE_CAMERA_CAPTURE = SCHED_PRIORITY_NORMAL,
  //
  NICE_CAMERA_SHUTTER_CB = SCHED_PRIORITY_NORMAL,
  NICE_CAMERA_ZIP_IMAGE_CB = SCHED_PRIORITY_NORMAL,
  NICE_CAMERA_FRAMEWORK_CB = SCHED_PRIORITY_NORMAL,
  //
  //
  NICE_CAMERA_EIS = SCHED_PRIORITY_NORMAL,
  NICE_CAMERA_VHDR = SCHED_PRIORITY_NORMAL,
  // Lomo Jni for Matrix Menu of Effect
  NICE_CAMERA_LOMO = SCHED_PRIORITY_NORMAL - 8,
  //
  //  3A-related
  NICE_CAMERA_3A_MAIN = (SCHED_PRIORITY_NORMAL - 7),
  NICE_CAMERA_AE = (SCHED_PRIORITY_NORMAL - 8),
  NICE_CAMERA_AF = (SCHED_PRIORITY_NORMAL - 20),
  NICE_CAMERA_FLK = (SCHED_PRIORITY_NORMAL - 3),
  NICE_CAMERA_TSF = (SCHED_PRIORITY_NORMAL - 8),
  NICE_CAMERA_STT_AF = (SCHED_PRIORITY_NORMAL - 20),
  NICE_CAMERA_STT = (SCHED_PRIORITY_NORMAL - 8),
  NICE_CAMERA_CCU = (SCHED_PRIORITY_NORMAL - 4),
  NICE_CAMERA_AE_Start = (SCHED_PRIORITY_NORMAL - 7),
  NICE_CAMERA_AF_Start = (SCHED_PRIORITY_NORMAL - 7),
  NICE_CAMERA_ISP_BPCI = (SCHED_PRIORITY_NORMAL - 7),
  NICE_CAMERA_CONFIG_STTPIPE = (SCHED_PRIORITY_NORMAL - 7),
  NICE_CAMERA_ResultPool = (SCHED_PRIORITY_NORMAL - 4),
  //
  // Pipeline-related
  NICE_CAMERA_PIPELINE_P1NODE = (SCHED_PRIORITY_NORMAL - 4),
  //
  NICE_CAMERA_PIPEMGR_BASE = SCHED_PRIORITY_FOREGROUND,
};

/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_PRIORITYDEFS_H_
