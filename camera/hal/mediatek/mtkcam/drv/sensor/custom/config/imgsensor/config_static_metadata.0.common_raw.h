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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_CUSTOM_CONFIG_IMGSENSOR_CONFIG_STATIC_METADATA_0_COMMON_RAW_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_CUSTOM_CONFIG_IMGSENSOR_CONFIG_STATIC_METADATA_0_COMMON_RAW_H_

STATIC_METADATA_BEGIN(0, COMMON_RAW)
//------------------------------------------------------------------------------
//==========================================================================
CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_ACTIVE_ARRAY_REGION)
CONFIG_ENTRY_VALUE(MRect(MPoint(0, 0), MSize(3200, 2400)), MRect);
CONFIG_METADATA_END()
//==========================================================================
CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT)
CONFIG_ENTRY_VALUE(MTK_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_BGGR, MUINT8);
CONFIG_METADATA_END()
//==========================================================================
CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_PHYSICAL_SIZE)
CONFIG_ENTRY_VALUE(3.20f, MFLOAT);  // millimeters
CONFIG_ENTRY_VALUE(2.40f, MFLOAT);  // millimeters
CONFIG_METADATA_END()
//==========================================================================
CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_PIXEL_ARRAY_SIZE)
CONFIG_ENTRY_VALUE(MSize(3200, 2400), MSize);
CONFIG_METADATA_END()
//==========================================================================
CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_WHITE_LEVEL)
CONFIG_ENTRY_VALUE(4000, MINT32);
CONFIG_METADATA_END()
//==========================================================================
CONFIG_METADATA_BEGIN(MTK_SENSOR_INFO_PACKAGE)
//----------------------------------------------------------------------
CONFIG_ENTRY_METADATA(
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_SCENARIO_ID)
        CONFIG_ENTRY_VALUE(MTK_SENSOR_INFO_SCENARIO_ID_ZSD, MINT32);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_FRAME_RATE)
        CONFIG_ENTRY_VALUE(15, MINT32);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_OUTPUT_REGION_ON_ACTIVE_ARRAY)
        CONFIG_ENTRY_VALUE(MRect(MPoint(0, 0), MSize(3200, 2400)), MRect);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_REAL_OUTPUT_SIZE)
        CONFIG_ENTRY_VALUE(MSize(3200, 2400), MSize);
    CONFIG_METADATA2_END()
    //..................................................................
)
//----------------------------------------------------------------------
CONFIG_ENTRY_METADATA(
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_SCENARIO_ID)
        CONFIG_ENTRY_VALUE(MTK_SENSOR_INFO_SCENARIO_ID_NORMAL_PREVIEW, MINT32);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_FRAME_RATE)
        CONFIG_ENTRY_VALUE(30, MINT32);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_OUTPUT_REGION_ON_ACTIVE_ARRAY)
        CONFIG_ENTRY_VALUE(MRect(MPoint(0, 0), MSize(3200, 2400)), MRect);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_REAL_OUTPUT_SIZE)
        CONFIG_ENTRY_VALUE(MSize(1600, 1200), MSize);
    CONFIG_METADATA2_END()
    //..................................................................
)
//----------------------------------------------------------------------
CONFIG_ENTRY_METADATA(
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_SCENARIO_ID)
        CONFIG_ENTRY_VALUE(MTK_SENSOR_INFO_SCENARIO_ID_NORMAL_CAPTURE, MINT32);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_FRAME_RATE)
        CONFIG_ENTRY_VALUE(15, MINT32);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_OUTPUT_REGION_ON_ACTIVE_ARRAY)
        CONFIG_ENTRY_VALUE(MRect(MPoint(0, 0), MSize(3200, 2400)), MRect);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_REAL_OUTPUT_SIZE)
        CONFIG_ENTRY_VALUE(MSize(3200, 2400), MSize);
    CONFIG_METADATA2_END()
    //..................................................................
)
//----------------------------------------------------------------------
CONFIG_ENTRY_METADATA(
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_SCENARIO_ID)
        CONFIG_ENTRY_VALUE(MTK_SENSOR_INFO_SCENARIO_ID_NORMAL_VIDEO, MINT32);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_FRAME_RATE)
        CONFIG_ENTRY_VALUE(30, MINT32);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_OUTPUT_REGION_ON_ACTIVE_ARRAY)
        CONFIG_ENTRY_VALUE(MRect(MPoint(540, 405), MSize(2120, 1590)), MRect);
    CONFIG_METADATA2_END()
    //..................................................................
    CONFIG_METADATA2_BEGIN(MTK_SENSOR_INFO_REAL_OUTPUT_SIZE)
        CONFIG_ENTRY_VALUE(MSize(2120, 1590), MSize);
    CONFIG_METADATA2_END()
    //..................................................................
)
//----------------------------------------------------------------------
CONFIG_METADATA_END()
//==========================================================================
//------------------------------------------------------------------------------
STATIC_METADATA_END()

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_CUSTOM_CONFIG_IMGSENSOR_CONFIG_STATIC_METADATA_0_COMMON_RAW_H_
