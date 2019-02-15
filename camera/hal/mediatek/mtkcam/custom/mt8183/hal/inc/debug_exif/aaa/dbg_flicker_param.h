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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_AAA_DBG_FLICKER_PARAM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_AAA_DBG_FLICKER_PARAM_H_

// Flicker debug info
#define FLICKER_DEBUG_TAG_VERSION (0)
#define FLICKER_DEBUG_TAG_SUBVERSION (0)
#define FLICKER_DEBUG_TAG_VERSION_DP \
  ((FLICKER_DEBUG_TAG_SUBVERSION << 16) | FLICKER_DEBUG_TAG_VERSION)
#define FLICKER_DEBUG_TAG_SIZE 100

typedef struct {
  AAA_DEBUG_TAG_T Tag[FLICKER_DEBUG_TAG_SIZE];
} FLICKER_DEBUG_INFO_T;

// flicker Debug Tag
enum {
  // flicker
  // BEGIN_OF_EXIF_TAG
  FLICKER_TAG_VERSION = 0,
  FLICKER_TAG_MAX
  // END_OF_EXIF_TAG
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_AAA_DBG_FLICKER_PARAM_H_
