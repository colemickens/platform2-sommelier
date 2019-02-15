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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_AAA_DBG_AAA_COMMON_PARAM_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_AAA_DBG_AAA_COMMON_PARAM_H_

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 3A debug info
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define AAATAG(module_id, tag, line_keep)                                 \
  ((MINT32)((MUINT32)(0x00000000) | (MUINT32)((module_id & 0xff) << 24) | \
            (MUINT32)((line_keep & 0x01) << 23) | (MUINT32)(tag & 0xffff)))

#define MODULE_NUM(total_module, tag_module)                                 \
  ((MINT32)((MUINT32)(0x00000000) | (MUINT32)((total_module & 0xff) << 16) | \
            (MUINT32)(tag_module & 0xff)))

typedef struct {
  MUINT32 u4FieldID;
  MUINT32 u4FieldValue;
} AAA_DEBUG_TAG_T;

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_DEBUG_EXIF_AAA_DBG_AAA_COMMON_PARAM_H_
