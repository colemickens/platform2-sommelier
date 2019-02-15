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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_MT8183_SENINF_DRV_DEF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_MT8183_SENINF_DRV_DEF_H_

#include <mtkcam/drv/IHalSensor.h>
#include "mtkcam/custom/mt8183/hal/inc/camera_custom_imgsensor_cfg.h"
#include "mtkcam/custom/mt8183/kernel/imgsensor/kd_imgsensor_define.h"

using NSCamCustomSensor::CUSTOM_CFG_CSI_PORT;

/******************************************************************************
 *
 ******************************************************************************/
typedef enum {
  SENINF_MUX1 = 0x0,
  SENINF_MUX2 = 0x1,
  SENINF_MUX3 = 0x2,
  SENINF_MUX4 = 0x3,
  SENINF_MUX5 = 0x4,
  SENINF_MUX6 = 0x5,
  SENINF_MUX_NUM,
  SENINF_MUX_ERROR = -1,
} SENINF_MUX_ENUM;

typedef enum {
  SENINF_1 = 0x0,
  SENINF_2 = 0x1,
  SENINF_3 = 0x2,
  SENINF_4 = 0x3,
  SENINF_5 = 0x4,
  SENINF_NUM,
} SENINF_ENUM;

typedef enum {
  PAD_10BIT = 0x0,
  PAD_8BIT_7_0 = 0x3,
  PAD_8BIT_9_2 = 0x4,
} PAD2CAM_DATA_ENUM;

typedef enum {  // 0:CSI2(2.5G), 3: parallel, 8:NCSI2(1.5G)
  CSI2 = 0x0,   /* 2.5G support */
  TEST_MODEL = 0x1,
  CCIR656 = 0x2,
  PARALLEL_SENSOR = 0x3,
  SERIAL_SENSOR = 0x4,
  HD_TV = 0x5,
  EXT_CSI2_OUT1 = 0x6,
  EXT_CSI2_OUT2 = 0x7,
  MIPI_SENSOR = 0x8, /* 1.5G support */
  VIRTUAL_CHANNEL_1 = 0x9,
  VIRTUAL_CHANNEL_2 = 0xA,
  VIRTUAL_CHANNEL_3 = 0xB,
  VIRTUAL_CHANNEL_4 = 0xC,
  VIRTUAL_CHANNEL_5 = 0xD,
  VIRTUAL_CHANNEL_6 = 0xE,
} SENINF_SOURCE_ENUM;

typedef enum {          // 0:CSI2(2.5G), 1:NCSI2(1.5G)
  CSI2_1_5G = 0x0,      /* 1.5G support */
  CSI2_2_5G = 0x1,      /* 2.5G support*/
  CSI2_2_5G_CPHY = 0x2, /* 2.5G support*/
} SENINF_CSI2_ENUM;

typedef enum {
  RAW_8BIT_FMT = 0x0,
  RAW_10BIT_FMT = 0x1,
  RAW_12BIT_FMT = 0x2,
  YUV422_FMT = 0x3,
  RAW_14BIT_FMT = 0x4,
  RGB565_MIPI_FMT = 0x5,
  RGB888_MIPI_FMT = 0x6,
  JPEG_FMT = 0x7
} TG_FORMAT_ENUM;

typedef enum {
  CMD_SENINF_GET_SENINF_ADDR,
  CMD_SENINF_DEBUG_TASK,
  CMD_SENINF_DEBUG_TASK_CAMSV,
  CMD_SENINF_DEBUG_PIXEL_METER,
  CMD_SENINF_MAX
} CMD_SENINF;

typedef struct {
  CUSTOM_CFG_CSI_PORT port;
  SENINF_ENUM seninf;
  SENINF_SOURCE_ENUM srcType;
} SENINF_CSI_INFO;

typedef struct {
  unsigned int enable;
  SENINF_CSI_INFO* pCsiInfo;
  SENINF_CSI2_ENUM csi_type;
  unsigned int dlaneNum;
  unsigned int dpcm;
  unsigned int dataheaderOrder;
  unsigned int padSel;
} SENINF_CSI_MIPI;

typedef struct {
  MUINT enable;
  MUINT SCAM_DataNumber;     // 0: NCSI2 , 1:CSI2
  MUINT SCAM_DDR_En;         // 0: Enable HS Detect, 1: disable HS Detect
  MUINT SCAM_CLK_INV;        // Enable DPCM mode type
  MUINT SCAM_DEFAULT_DELAY;  // default delay for calibration
  MUINT SCAM_CRC_En;
  MUINT SCAM_SOF_src;
  MUINT SCAM_Timout_Cali;
} SENINF_CSI_SCAM;

typedef struct {
  MUINT enable;
} SENINF_CSI_PARALLEL;

typedef struct {
  MUINT32 mclkIdx;
  MUINT32 mclkFreq;
  MBOOL mclkPolarityLow;
  MUINT8 mclkRisingCnt;
  MUINT8 mclkFallingCnt;
  MUINT32 pclkInv;
  MUINT32 mclkPLL;
  IMGSENSOR_SENSOR_IDX sensorIdx;
} SENINF_MCLK_PARA;

#define SENINF_CAM_MUX_MIN SENINF_MUX1
#define SENINF_CAM_MUX_MAX SENINF_MUX3
#define SENINF_CAMSV_MUX_MIN SENINF_MUX3
#define SENINF_CAMSV_MUX_MAX SENINF_MUX_NUM

#define SENINF_PIXEL_MODE_CAM FOUR_PIXEL_MODE
#define SENINF_PIXEL_MODE_CAMSV FOUR_PIXEL_MODE

#define SENINF_TIMESTAMP_CLK 1000

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_MT8183_SENINF_DRV_DEF_H_
