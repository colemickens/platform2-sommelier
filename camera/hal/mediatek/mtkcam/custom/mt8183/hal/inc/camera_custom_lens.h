
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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_LENS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_LENS_H_

// #include "../../../../inc/MediaTypes.h"
// #include "msdk_nvram_camera_exp.h"
// #include "camera_custom_nvram.h"

#define MAX_NUM_OF_SUPPORT_LENS 16

#define DUMMY_SENSOR_ID 0xFFFF

#define DUMMY_MODULE_ID 0x0

/* LENS ID */
#define DUMMY_LENS_ID 0xFFFF
#define FM50AF_LENS_ID 0x0001
#define MT9P017AF_LENS_ID 0x0002

#define SENSOR_DRIVE_LENS_ID 0x1000
#define GAF001AF_LENS_ID 0xFF01
#define GAF002AF_LENS_ID 0xFF02
#define GAF008AF_LENS_ID 0xFF08

#define OV8825AF_LENS_ID 0x0003
#define BU6429AF_LENS_ID 0x0004
#define BU6424AF_LENS_ID 0x0005
#define BU64748AF_LENS_ID 0x6474
#define BU63165AF_LENS_ID 0x6316
#define BU63169AF_LENS_ID 0x6369
#define AD5823AF_LENS_ID 0x5823
#define DW9718AF_LENS_ID 0x9718
#define DW9718SAF_LENS_ID 0x9719
#define DW9800WAF_LENS_ID 0x9800
#define AD5820AF_LENS_ID 0x5820
#define DW9714AF_LENS_ID 0x9714
#define LC898122AF_LENS_ID 0x9812
#define LC898212AF_LENS_ID 0x8212
#define LC898212XDAF_LENS_ID 0x8213
#define LC898214AF_LENS_ID 0x8214
#define LC898217AF_LENS_ID 0x8217
#define BU64745GWZAF_LENS_ID 0xFCE9
#define RUMBAAF_LENS_ID 0x6334
#define AK7348AF_LENS_ID 0x7348
#define AK7371AF_LENS_ID 0x7371
/* AF LAMP THRESHOLD*/
#define AF_LAMP_LV_THRES 60

typedef unsigned int (*PFUNC_GETLENSDEFAULT)(void*, unsigned int);

typedef struct {
  unsigned int SensorId;
  unsigned int ModuleId;
  unsigned int LensId;
  unsigned char LensDrvName[32];
  unsigned int (*getLensDefault)(void* pDataBuf, unsigned int size);
} MSDK_LENS_INIT_FUNCTION_STRUCT, *PMSDK_LENS_INIT_FUNCTION_STRUCT;

unsigned int GetLensInitFuncList(PMSDK_LENS_INIT_FUNCTION_STRUCT pLensList,
                                 unsigned int a_u4CurrSensorDev);

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_HAL_INC_CAMERA_CUSTOM_LENS_H_
