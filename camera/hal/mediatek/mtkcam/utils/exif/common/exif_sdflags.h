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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_EXIF_SDFLAGS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_EXIF_SDFLAGS_H_

/****************************************************************************
 *
 *  exif_sdflags.h -- general purpose stereoscopic descriptor
 *
 ****************************************************************************/

/* Media Types */
#define SD_MTYPE_MONOSCOPIC_IMAGE 0x00
#define SD_MTYPE_STEREOSCOPIC_IMAGE 0x01

/* layout Options */
#define SD_LAYOUT_INTERLEAVED 0x01
#define SD_LAYOUT_SIDEBYSIDE 0x02
#define SD_LAYOUT_OVERUNDER 0x03
#define SD_LAYOUT_ANAGLYPH 0x04

/* Misc Flags Bits */
#define SD_FULL_HEIGHT 0x00
#define SD_HALF_HEIGHT 0x01
#define SD_FULL_WIDTH 0x00
#define SD_HALF_WIDTH 0x02
#define SD_RIGHT_FIELD_FIRST 0x00
#define SD_LEFT_FIELD_FIRST 0x04

/* Defines for SD_MTYPE_MONOSCOPIC_IMAGE */
#define SD_EYE_BOTH 0x0000
#define SD_EYE_LEFT 0x0100
#define SD_EYE_RIGHT 0x0200

#endif  //  CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_EXIF_SDFLAGS_H_
