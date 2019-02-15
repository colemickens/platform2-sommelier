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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_SENSOR_PARAM0_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_SENSOR_PARAM0_H_

/******************************************************************************
 *
 ******************************************************************************/
#include "../dbg_exif_def.h"

namespace dbg_cam_sensor_param_0 {
/******************************************************************************
 *
 ******************************************************************************/

// Sensor debug info
enum { SENSOR_DEBUG_TAG_VERSION = 0 };
enum { SENSOR_DEBUG_TAG_SUBVERSION = 0 };
const unsigned int SENSOR_DEBUG_TAG_VERSION_DP =
    ((SENSOR_DEBUG_TAG_SUBVERSION << 16) | SENSOR_DEBUG_TAG_VERSION);
enum { SENSOR_DEBUG_TAG_SIZE = 22 };

typedef struct DEBUG_SENSOR_INFO_S {
  debug_exif_field Tag[SENSOR_DEBUG_TAG_SIZE];
} DEBUG_SENSOR_INFO_T;

// Common Parameter Structure
typedef enum {
  // BEGIN_OF_EXIF_TAG
  SENSOR_TAG_VERSION = 0,
  SENSOR1_TAG_COLORORDER,  // 0:B , 1:Gb, 2:Gr, 3:R, 4:UYVY, 5:VYUY, 6:YUYV,
                           // 7:YVYU
  SENSOR1_TAG_DATATYPE,    // 0:RAW, 1:YUV, 2:YCBCR, 3:RGB565, 4:RGB888, 5:JPEG
  SENSOR1_TAG_HARDWARE_INTERFACE,  // 0: parallel, 1:MIPI
  SENSOR1_TAG_GRAB_START_X,
  SENSOR1_TAG_GRAB_START_Y,
  SENSOR1_TAG_GRAB_WIDTH,
  SENSOR1_TAG_GRAB_HEIGHT,
  SENSOR2_TAG_COLORORDER,  // 0:B , 1:Gb, 2:Gr, 3:R, 4:UYVY, 5:VYUY, 6:YUYV,
                           // 7:YVYU
  SENSOR2_TAG_DATATYPE,    // 0:RAW, 1:YUV, 2:YCBCR, 3:RGB565, 4:RGB888, 5:JPEG
  SENSOR2_TAG_HARDWARE_INTERFACE,  // 0: parallel, 1:MIPI
  SENSOR2_TAG_GRAB_START_X,
  SENSOR2_TAG_GRAB_START_Y,
  SENSOR2_TAG_GRAB_WIDTH,
  SENSOR2_TAG_GRAB_HEIGHT,
  SENSOR3_TAG_COLORORDER,  // 0:B , 1:Gb, 2:Gr, 3:R, 4:UYVY, 5:VYUY, 6:YUYV,
                           // 7:YVYU
  SENSOR3_TAG_DATATYPE,    // 0:RAW, 1:YUV, 2:YCBCR, 3:RGB565, 4:RGB888, 5:JPEG
  SENSOR3_TAG_HARDWARE_INTERFACE,  // 0: parallel, 1:MIPI
  SENSOR3_TAG_GRAB_START_X,
  SENSOR3_TAG_GRAB_START_Y,
  SENSOR3_TAG_GRAB_WIDTH,
  SENSOR3_TAG_GRAB_HEIGHT,
  // END_OF_EXIF_TAG
} DEBUG_SENSOR_TAG_T;

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace dbg_cam_sensor_param_0

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CUSTOM_DEBUG_EXIF_CAM_DBG_CAM_SENSOR_PARAM0_H_
