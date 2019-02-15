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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_SENINF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_SENINF_H_

#include <kd_imgsensor_define.h>
#include <kd_seninf_define.h>

#define SENINFMAGIC 's'
/* IOCTRL(inode * ,file * ,cmd ,arg ) */
/* S means "set through a ptr" */
/* T means "tell by a arg value" */
/* G means "get by a ptr" */
/* Q means "get by return a value" */
/* X means "switch G and S atomically" */
/* H means "switch T and Q atomically" */

#define KDSENINFIOC_X_GET_REG_ADDR _IOWR(SENINFMAGIC, 40, struct KD_SENINF_REG)
#define KDSENINFIOC_X_SET_MCLK_PLL \
  _IOWR(SENINFMAGIC, 60, ACDK_SENSOR_MCLK_STRUCT)
#define KDSENINFIOC_X_GET_ISP_CLK _IOWR(SENINFMAGIC, 80, u32)
#define KDSENINFIOC_X_GET_CSI_CLK _IOWR(SENINFMAGIC, 85, u32)

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_SENINF_H_
