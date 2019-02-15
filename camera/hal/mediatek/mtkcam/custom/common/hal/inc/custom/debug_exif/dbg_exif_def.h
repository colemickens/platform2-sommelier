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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_DBG_EXIF_DEF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_DBG_EXIF_DEF_H_
#pragma once

/******************************************************************************
 *
 ******************************************************************************/
#include <cstdint>
#include <map>
//
#include "custom/debug_exif/dbg_id_param.h"

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/**
 *
 */
struct debug_exif_field {
  uint32_t u4FieldID;
  uint32_t u4FieldValue;
};

/**
 *
 */
struct debug_exif_module_info {
  uint32_t module_id;  // module id.
  uint32_t version;    // module verson.
  uint32_t size;       // module size, in bytes.
  uint32_t offset;     // module offset.
};

/*****************************************************************************
 * @brief the buffer info.
 *
 * @note
 *  header_size and body_size are the size of buffer header, in bytes, and the
 *  size of buffer body, in bytes, respectively.
 *  The buffer size = head_size + body_size.
 *
 ******************************************************************************/
struct debug_exif_buffer_info {
  uint32_t header_size;
  uint32_t body_size;

  void const* header_context;

  std::map<uint32_t,  // module_id
           debug_exif_module_info>
      body_layout;
};

/******************************************************************************
 *
 ******************************************************************************/
/**
 *  |   8  |       8      |   8  |     8      |
 *  | 0x00 | total_module | 0x00 | tag_module |
 */
#define DBGEXIF_MODULE_NUM(total_module, tag_module) \
  ((uint32_t)((total_module & 0xff) << 16) | (uint32_t)(tag_module & 0xff))

/**
 *  |     8     |      1    |   7  |    16    |
 *  | module_id | line_keep | 0x00 |  tag_id  |
 */
#define DBGEXIF_TAG(module_id, tag, line_keep) \
  ((uint32_t)((module_id & 0xff) << 24) |      \
   (uint32_t)((line_keep & 0x01) << 23) | (uint32_t)(tag & 0xffff))

#define N3DAAATAG DBGEXIF_TAG  // deprecated; (used by libn3d3a)

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_DBG_EXIF_DEF_H_
