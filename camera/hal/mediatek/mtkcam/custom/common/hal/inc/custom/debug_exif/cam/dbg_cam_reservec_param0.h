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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_RESERVEC_PARAM0_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_RESERVEC_PARAM0_H_

/******************************************************************************
 *
 ******************************************************************************/
#include "../dbg_exif_def.h"

namespace dbg_cam_reservec_param_0 {
/******************************************************************************
 *
 ******************************************************************************/

// ReserveC Parameter Structure
typedef enum {
  RESERVEC_TAG_VERSION = 0,
  /* add tags here */

  RESERVEC_TAG_END
} DEBUG_RESERVEC_TAG_T;

// TEST_C debug info
enum { RESERVEC_DEBUG_TAG_VERSION = 0 };
enum { RESERVEC_DEBUG_NON_TAG_VAL_SIZE = 10000 };
enum {
  RESERVEC_DEBUG_TAG_SIZE = (RESERVEC_TAG_END + RESERVEC_DEBUG_NON_TAG_VAL_SIZE)
};

// gmv
enum { MF_MAX_FRAME = 8 };
enum {
  MF_GMV_DEBUG_TAG_GMV_X,
  MF_GMV_DEBUG_TAG_GMV_Y,
  MF_GMV_DEBUG_TAG_ITEM_SIZE
};
enum { MF_GMV_DEBUG_TAG_SIZE = (MF_GMV_DEBUG_TAG_ITEM_SIZE) };

// eis
enum { MF_EIS_DEBUG_TAG_WINDOW = 32 };
enum {
  MF_EIS_DEBUG_TAG_MV_X,
  MF_EIS_DEBUG_TAG_MV_Y,
  MF_EIS_DEBUG_TAG_TRUST_X,
  MF_EIS_DEBUG_TAG_TRUST_Y,
  MF_EIS_DEBUG_TAG_ITEM_SIZE
};
enum {
  MF_EIS_DEBUG_TAG_SIZE = (MF_EIS_DEBUG_TAG_WINDOW * MF_EIS_DEBUG_TAG_ITEM_SIZE)
};

struct DEBUG_RESERVEC_INFO_T {
  uint32_t Tag[RESERVEC_DEBUG_TAG_SIZE];
  const uint32_t count;
  const uint32_t gmvCount;
  const uint32_t eisCount;
  const uint32_t gmvSize;
  const uint32_t eisSize;
  int32_t gmvData[MF_MAX_FRAME][MF_GMV_DEBUG_TAG_ITEM_SIZE];
  uint32_t eisData[MF_MAX_FRAME][MF_EIS_DEBUG_TAG_WINDOW]
                  [MF_EIS_DEBUG_TAG_ITEM_SIZE];

  DEBUG_RESERVEC_INFO_T()
      : count(2)  // gmvCount + eisCount
                  //
        ,
        gmvCount(MF_MAX_FRAME),
        eisCount(MF_MAX_FRAME)
        //
        ,
        gmvSize(MF_GMV_DEBUG_TAG_SIZE),
        eisSize(MF_EIS_DEBUG_TAG_SIZE) {}
};

struct DEBUG_RESERVEC_INFO_S {
  debug_exif_field Tag[RESERVEC_DEBUG_TAG_SIZE];
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace dbg_cam_reservec_param_0

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_RESERVEC_PARAM0_H_
