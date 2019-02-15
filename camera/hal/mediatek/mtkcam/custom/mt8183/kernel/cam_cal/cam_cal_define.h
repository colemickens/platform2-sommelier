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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_CAM_CAL_CAM_CAL_DEFINE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_CAM_CAL_CAM_CAL_DEFINE_H_

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/compat.h>
#include <linux/fs.h>
#endif

struct stCAM_CAL_INFO_STRUCT {
  u32 u4Offset;
  u32 u4Length;
  u32 sensorID;
  u32 deviceID; /* MAIN = 0x01, SUB  = 0x02, MAIN_2 = 0x04, SUB_2 = 0x08 */
  u8* pu1Params;
};

#ifdef CONFIG_COMPAT

struct COMPAT_stCAM_CAL_INFO_STRUCT {
  u32 u4Offset;
  u32 u4Length;
  u32 sensorID;
  u32 deviceID;
  compat_uptr_t pu1Params;
};
#endif

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_CAM_CAL_CAM_CAL_DEFINE_H_
