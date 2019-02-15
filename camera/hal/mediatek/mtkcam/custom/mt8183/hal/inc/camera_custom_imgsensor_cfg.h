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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_IMGSENSOR_CFG_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_IMGSENSOR_CFG_H_

#include <camera_custom_types.h>
#include <kd_camera_feature.h>
#include <mtkcam/def/common.h>

namespace NSCamCustomSensor {
/*******************************************************************************
 * Sensor mclk usage
 ******************************************************************************/
typedef enum {
  CUSTOM_CFG_MCLK_1 = 0x0,  // mclk1
  CUSTOM_CFG_MCLK_2,        // mclk2
  CUSTOM_CFG_MCLK_3,        // mclk3
  CUSTOM_CFG_MCLK_4,
  CUSTOM_CFG_MCLK_5,
  CUSTOM_CFG_MCLK_MAX_NUM,
  CUSTOM_CFG_MCLK_NONE
} CUSTOM_CFG_MCLK;

/*******************************************************************************
 * MIPI sensor pad usage
 ******************************************************************************/
typedef enum {
  CUSTOM_CFG_CSI_PORT_0 = 0x0,  // 4D1C
  CUSTOM_CFG_CSI_PORT_1,        // 4D1C
  CUSTOM_CFG_CSI_PORT_2,        // 4D1C
  CUSTOM_CFG_CSI_PORT_0A,       // 2D1C
  CUSTOM_CFG_CSI_PORT_0B,       // 2D1C
  CUSTOM_CFG_CSI_PORT_MAX_NUM,
  CUSTOM_CFG_CSI_PORT_NONE  // for non-MIPI sensor
} CUSTOM_CFG_CSI_PORT;

/*******************************************************************************
 * Sensor Placement Facing Direction
 *
 *   CUSTOM_CFG_DIR_REAR  : Back side
 *   CUSTOM_CFG_DIR_FRONT : Front side (LCD side)
 ******************************************************************************/
typedef enum {
  CUSTOM_CFG_DIR_REAR,
  CUSTOM_CFG_DIR_FRONT,
  CUSTOM_CFG_DIR_MAX_NUM,
  CUSTOM_CFG_DIR_NONE
} CUSTOM_CFG_DIR;

/*******************************************************************************
 * Sensor Input Data Bit Order
 *
 *   CUSTOM_CFG_BITORDER_9_2 : raw data input [9:2]
 *   CUSTOM_CFG_BITORDER_7_0 : raw data input [7:0]
 ******************************************************************************/
typedef enum {
  CUSTOM_CFG_BITORDER_9_2,
  CUSTOM_CFG_BITORDER_7_0,
  CUSTOM_CFG_BITORDER_MAX_NUM,
  CUSTOM_CFG_BITORDER_NONE
} CUSTOM_CFG_BITORDER;

typedef struct {
  IMGSENSOR_SENSOR_IDX sensorIdx;
  CUSTOM_CFG_MCLK mclk;
  CUSTOM_CFG_CSI_PORT port;
  CUSTOM_CFG_DIR dir;
  CUSTOM_CFG_BITORDER bitOrder;
  MUINT32 orientation;
  MUINT32 horizontalFov;
  MUINT32 verticalFov;
} CUSTOM_CFG;

/*******************************************************************************
 * Get Custom Configuration
 ******************************************************************************/
CUSTOM_CFG* VISIBILITY_PUBLIC
getCustomConfig(IMGSENSOR_SENSOR_IDX const sensorIdx);

};      // namespace NSCamCustomSensor
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_IMGSENSOR_CFG_H_
