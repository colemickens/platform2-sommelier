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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CFG_CAMERA_FILE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CFG_CAMERA_FILE_H_

#include "CFG_Camera_File_Max_Size.h"

typedef unsigned char UINT8;

typedef struct {
  UINT8 CameraData[MAXIMUM_NVRAM_CAMERA_SENSOR_FILE_SIZE];
} NVRAM_SENSOR_DATA_STRUCT, *PNVRAM_SENSOR_DATA_STRUCT;
typedef struct {
  UINT8 CameraData[MAXIMUM_NVRAM_CAMERA_PARA_FILE_SIZE];
} NVRAM_CAMERA_PARA_STRUCT, *PNVRAM_CAMERA_PARA_STRUCT;

typedef struct {
  UINT8 Data[MAXIMUM_NVRAM_CAMERA_DEFECT_FILE_SIZE];
} NVRAM_CAMERA_DEFECT_STRUCT, *PNVRAM_CAMERA_DEFECT_STRUCT;

/*******************************************************************************
 * shading
 ******************************************************************************/

typedef struct {
  UINT8 CameraData[MAXIMUM_NVRAM_CAMERA_SHADING_FILE_SIZE];
} NVRAM_CAMERA_SHADING_STRUCT, *PNVRAM_CAMERA_SHADING_STRUCT;

/*******************************************************************************
 * 3A
 ******************************************************************************/

typedef struct {
  UINT8 Data[MAXIMUM_NVRAM_CAMERA_3A_FILE_SIZE];
} NVRAM_CAMERA_3A_STRUCT, *PNVRAM_CAMERA_3A_STRUCT;

/*******************************************************************************
 * ISP parameter
 ******************************************************************************/

typedef struct {
  UINT8 Data[MAXIMUM_NVRAM_CAMERA_ISP_FILE_SIZE];
} NVRAM_CAMERA_ISP_PARAM_STRUCT, *PNVRAM_CAMERA_ISP_PARAM_STRUCT;

/*******************************************************************************
 * Lens
 ******************************************************************************/

typedef struct {
  UINT8 reserved[MAXIMUM_NVRAM_CAMERA_LENS_FILE_SIZE];
} NVRAM_LENS_PARA_STRUCT, *PNVRAM_LENS_PARA_STRUCT;

/* define the LID and total record for NVRAM interface */
#define CFG_FILE_CAMERA_PARA_REC_SIZE MAXIMUM_NVRAM_CAMERA_ISP_FILE_SIZE
#define CFG_FILE_CAMERA_3A_REC_SIZE MAXIMUM_NVRAM_CAMERA_3A_FILE_SIZE
#define CFG_FILE_CAMERA_SHADING_REC_SIZE MAXIMUM_NVRAM_CAMERA_SHADING_FILE_SIZE
#define CFG_FILE_CAMERA_DEFECT_REC_SIZE MAXIMUM_NVRAM_CAMERA_DEFECT_FILE_SIZE
#define CFG_FILE_CAMERA_SENSOR_REC_SIZE MAXIMUM_NVRAM_CAMERA_SENSOR_FILE_SIZE
#define CFG_FILE_CAMERA_LENS_REC_SIZE MAXIMUM_NVRAM_CAMERA_LENS_FILE_SIZE
#define CFG_FILE_CAMERA_VERSION_REC_SIZE MAXIMUM_NVRAM_CAMERA_VERSION_FILE_SIZE
#define CFG_FILE_CAMERA_FEATURE_REC_SIZE MAXIMUM_NVRAM_CAMERA_FEATURE_FILE_SIZE
#define CFG_FILE_CAMERA_GEOMETRY_REC_SIZE \
  MAXIMUM_NVRAM_CAMERA_GEOMETRY_FILE_SIZE

#define CFG_FILE_CAMERA_PLINE_REC_SIZE MAXIMUM_NVRAM_CAMERA_PLINE_FILE_SIZE
#define CFG_FILE_CAMERA_AF_REC_SIZE MAXIMUM_NVRAM_CAMERA_AF_FILE_SIZE
#define CFG_FILE_CAMERA_FLASH_CALIBRATION_REC_SIZE \
  MAXIMUM_NVRAM_CAMERA_FLASH_CALIBRATION_FILE_SIZE

#define CFG_FILE_CAMERA_PARA_REC_TOTAL 3
#define CFG_FILE_CAMERA_3A_REC_TOTAL 3
#define CFG_FILE_CAMERA_SHADING_REC_TOTAL 3
#define CFG_FILE_CAMERA_DEFECT_REC_TOTAL 3
#define CFG_FILE_CAMERA_SENSOR_REC_TOTAL 3
#define CFG_FILE_CAMERA_LENS_REC_TOTAL 3
#define CFG_FILE_CAMERA_VERSION_REC_TOTAL 1
#define CFG_FILE_CAMERA_FEATURE_REC_TOTAL 3
#define CFG_FILE_CAMERA_GEOMETRY_REC_TOTAL 3
#define CFG_FILE_CAMERA_PLINE_REC_TOTAL 3
#define CFG_FILE_CAMERA_AF_REC_TOTAL 3
#define CFG_FILE_CAMERA_FLASH_CALIBRATION_REC_TOTAL 3

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CFG_CAMERA_FILE_H_
