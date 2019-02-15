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

#include "./imgsensor_drv.h"
#include "mtkcam/custom/mt8183/hal/imgsensor_src/imgsensor_custom_info.h"
#include "mtkcam/custom/mt8183/kernel/imgsensor/kd_imgsensor_errcode.h"
#include <mtkcam/utils/std/Log.h>

MUINT32 getNumOfSupportSensor(void) {
  return sizeof(gImgsensor_info) / sizeof(struct imgsensor_info_struct);
}

SENSOR_WINSIZE_INFO_STRUCT* getImgWinSizeInfo(MUINT8 idx, MUINT32 scenario) {
  SENSOR_WINSIZE_INFO_STRUCT* pWinSizeInfo;

  if ((idx >= getNumOfSupportSensor()) && (scenario >= SCENARIO_ID_MAX)) {
    return NULL;
  }
  pWinSizeInfo = &gImgsensor_winsize_info[idx][scenario];
  return pWinSizeInfo;
}

struct imgsensor_info_struct* getImgsensorInfo(MUINT8 infoIdx) {
  struct imgsensor_info_struct* info;

  if (infoIdx >= getNumOfSupportSensor()) {
    return NULL;
  }
  info = &gImgsensor_info[infoIdx];
  return info;
}

MUINT32 getSensorListId(MUINT8 listIdx) {
  struct IMGSENSOR_SENSOR_LIST* psensor_list;

  if (listIdx >= getNumOfSupportSensor()) {
    return 0;
  }
  psensor_list = &gimgsensor_sensor_list[listIdx];
  return psensor_list->id;
}

const char* getSensorListName(MUINT8 listIdx) {
  char* pSensorName;
  struct IMGSENSOR_SENSOR_LIST* psensor_list;

  if (listIdx >= getNumOfSupportSensor()) {
    return NULL;
  }
  psensor_list = &gimgsensor_sensor_list[listIdx];
  pSensorName = reinterpret_cast<char*>(psensor_list->name);
  return pSensorName;
}

IMGSENSOR_SENSOR_LIST* getSensorList(MUINT8 listIdx) {
  struct IMGSENSOR_SENSOR_LIST* psensor_list;

  if (listIdx >= getNumOfSupportSensor()) {
    return NULL;
  }
  psensor_list = &gimgsensor_sensor_list[listIdx];
  return psensor_list;
}

MUINT32 getImgsensorType(MUINT8 infoIdx) {
  struct imgsensor_info_struct* info;

  if (infoIdx >= getNumOfSupportSensor()) {
    return IMAGE_SENSOR_TYPE_UNKNOWN;
  }

  info = &gImgsensor_info[infoIdx];

  if (info->sensor_output_dataformat >= SENSOR_OUTPUT_FORMAT_RAW_B &&
      info->sensor_output_dataformat <= SENSOR_OUTPUT_FORMAT_RAW_R) {
    return IMAGE_SENSOR_TYPE_RAW;
  } else if (info->sensor_output_dataformat >= SENSOR_OUTPUT_FORMAT_RAW8_B &&
             info->sensor_output_dataformat <= SENSOR_OUTPUT_FORMAT_RAW8_R) {
    return IMAGE_SENSOR_TYPE_RAW8;
  } else if (info->sensor_output_dataformat >= SENSOR_OUTPUT_FORMAT_UYVY &&
             info->sensor_output_dataformat <= SENSOR_OUTPUT_FORMAT_YVYU) {
    return IMAGE_SENSOR_TYPE_YUV;
  } else if (info->sensor_output_dataformat >= SENSOR_OUTPUT_FORMAT_CbYCrY &&
             info->sensor_output_dataformat <= SENSOR_OUTPUT_FORMAT_YCrYCb) {
    return IMAGE_SENSOR_TYPE_YCBCR;
  } else if (info->sensor_output_dataformat >= SENSOR_OUTPUT_FORMAT_RAW_RWB_B &&
             info->sensor_output_dataformat <= SENSOR_OUTPUT_FORMAT_RAW_RWB_R) {
    return IMAGE_SENSOR_TYPE_RAW;
  } else if (info->sensor_output_dataformat >=
                 SENSOR_OUTPUT_FORMAT_RAW_4CELL_B &&
             info->sensor_output_dataformat <=
                 SENSOR_OUTPUT_FORMAT_RAW_4CELL_R) {
    return IMAGE_SENSOR_TYPE_RAW;
  } else if (info->sensor_output_dataformat >=
                 SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B &&
             info->sensor_output_dataformat <=
                 SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R) {
    return IMAGE_SENSOR_TYPE_RAW;

  } else if (info->sensor_output_dataformat >=
                 SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_B &&
             info->sensor_output_dataformat <=
                 SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_R) {
    return IMAGE_SENSOR_TYPE_RAW;
  } else if (info->sensor_output_dataformat == SENSOR_OUTPUT_FORMAT_RAW_MONO) {
    return IMAGE_SENSOR_TYPE_RAW;
  } else {
    return IMAGE_SENSOR_TYPE_UNKNOWN;
  }
}
