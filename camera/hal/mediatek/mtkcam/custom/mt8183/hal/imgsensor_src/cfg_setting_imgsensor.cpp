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

#include <camera_custom_imgsensor_cfg.h>

namespace NSCamCustomSensor {

static CUSTOM_CFG gCustomCfg[] = {
    {.sensorIdx = IMGSENSOR_SENSOR_IDX_MAIN,
     .mclk = CUSTOM_CFG_MCLK_1,
     .port = CUSTOM_CFG_CSI_PORT_0,
     .dir = CUSTOM_CFG_DIR_REAR,
     .bitOrder = CUSTOM_CFG_BITORDER_9_2,
     .orientation = 90,
     .horizontalFov = 67,
     .verticalFov = 49},
    {.sensorIdx = IMGSENSOR_SENSOR_IDX_SUB,
     .mclk = CUSTOM_CFG_MCLK_2,
     .port = CUSTOM_CFG_CSI_PORT_1,
     .dir = CUSTOM_CFG_DIR_FRONT,
     .bitOrder = CUSTOM_CFG_BITORDER_9_2,
     .orientation = 90,
     .horizontalFov = 63,
     .verticalFov = 40},
    {.sensorIdx = IMGSENSOR_SENSOR_IDX_MAIN2,
     .mclk = CUSTOM_CFG_MCLK_3,
     .port = CUSTOM_CFG_CSI_PORT_2,
     .dir = CUSTOM_CFG_DIR_REAR,
     .bitOrder = CUSTOM_CFG_BITORDER_9_2,
     .orientation = 0,
     .horizontalFov = 75,
     .verticalFov = 60},
    {.sensorIdx = IMGSENSOR_SENSOR_IDX_SUB2,
     .mclk = CUSTOM_CFG_MCLK_3,
     .port = CUSTOM_CFG_CSI_PORT_2,
     .dir = CUSTOM_CFG_DIR_FRONT,
     .bitOrder = CUSTOM_CFG_BITORDER_9_2,
     .orientation = 0,
     .horizontalFov = 75,
     .verticalFov = 60},

    /* Add custom configuration before this line */
    {.sensorIdx = IMGSENSOR_SENSOR_IDX_NONE,
     .mclk = CUSTOM_CFG_MCLK_NONE,
     .port = CUSTOM_CFG_CSI_PORT_NONE,
     .dir = CUSTOM_CFG_DIR_NONE,
     .bitOrder = CUSTOM_CFG_BITORDER_NONE,
     .orientation = 0,
     .horizontalFov = 0,
     .verticalFov = 0}};

CUSTOM_CFG* getCustomConfig(IMGSENSOR_SENSOR_IDX const sensorIdx) {
  CUSTOM_CFG* pCustomCfg = &gCustomCfg[IMGSENSOR_SENSOR_IDX_MIN_NUM];

  if (sensorIdx >= IMGSENSOR_SENSOR_IDX_MAX_NUM ||
      sensorIdx < IMGSENSOR_SENSOR_IDX_MIN_NUM) {
    return NULL;
  }

  while (pCustomCfg->sensorIdx != IMGSENSOR_SENSOR_IDX_NONE &&
         pCustomCfg->sensorIdx != sensorIdx) {
    pCustomCfg++;
  }

  if (pCustomCfg->sensorIdx == IMGSENSOR_SENSOR_IDX_NONE) {
    return NULL;
  }

  return pCustomCfg;
}

};  // namespace NSCamCustomSensor
