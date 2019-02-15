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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_CUSTOM_CONFIG_IMGSENSOR_CONFIG_STATIC_METADATA_SETUP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_CUSTOM_CONFIG_IMGSENSOR_CONFIG_STATIC_METADATA_SETUP_H_

STATIC_METADATA_BEGIN(SETUP, COMMON)
//------------------------------------------------------------------------------
//==========================================================================
switch (rInfo.sensorIndex()) {
  case 0:
    //======================================================================
    CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_ORIENTATION)
    CONFIG_ENTRY_VALUE(90, MINT32);
    CONFIG_METADATA_END()
    //======================================================================
    CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_FACING)
    CONFIG_ENTRY_VALUE(0, MUINT8);
    CONFIG_METADATA_END()
    //======================================================================
    break;

  case 1:
    //======================================================================
    CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_ORIENTATION)
    CONFIG_ENTRY_VALUE(270, MINT32);
    CONFIG_METADATA_END()
    //======================================================================
    CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_FACING)
    CONFIG_ENTRY_VALUE(1, MUINT8);
    CONFIG_METADATA_END()
    //======================================================================
    break;

  default:
    break;
}
//==========================================================================
//------------------------------------------------------------------------------
STATIC_METADATA_END()

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_CUSTOM_CONFIG_IMGSENSOR_CONFIG_STATIC_METADATA_SETUP_H_
