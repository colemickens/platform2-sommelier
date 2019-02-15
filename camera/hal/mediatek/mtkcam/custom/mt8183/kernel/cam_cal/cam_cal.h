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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_CAM_CAL_CAM_CAL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_CAM_CAL_CAM_CAL_H_

#include <linux/ioctl.h>
#ifdef CONFIG_COMPAT
/*64 bit*/
#include <linux/compat.h>
#include <linux/fs.h>
#endif

#define CAM_CALAGIC 'i'
/*IOCTRL(inode * ,file * ,cmd ,arg )*/
/*S means "set through a ptr"*/
/*T means "tell by a arg value"*/
/*G means "get by a ptr"*/
/*Q means "get by return a value"*/
/*X means "switch G and S atomically"*/
/*H means "switch T and Q atomically"*/

/*******************************************************************************
 *
 ******************************************************************************/

/*CAM_CAL write*/
#define CAM_CALIOC_S_WRITE _IOW(CAM_CALAGIC, 0, struct stCAM_CAL_INFO_STRUCT)
/*CAM_CAL read*/
#define CAM_CALIOC_G_READ _IOWR(CAM_CALAGIC, 5, struct stCAM_CAL_INFO_STRUCT)

#ifdef CONFIG_COMPAT
#define COMPAT_CAM_CALIOC_S_WRITE \
  _IOW(CAM_CALAGIC, 0, struct COMPAT_stCAM_CAL_INFO_STRUCT)
#define COMPAT_CAM_CALIOC_G_READ \
  _IOWR(CAM_CALAGIC, 5, struct COMPAT_stCAM_CAL_INFO_STRUCT)
#endif
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_CAM_CAL_CAM_CAL_H_
