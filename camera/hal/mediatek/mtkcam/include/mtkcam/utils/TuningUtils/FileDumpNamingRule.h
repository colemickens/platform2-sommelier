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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_TUNINGUTILS_FILEDUMPNAMINGRULE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_TUNINGUTILS_FILEDUMPNAMINGRULE_H_

#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <memory>
#include <mtkcam/def/common.h>

namespace NSCam {
namespace TuningUtils {

enum RAW_PORT {
  RAW_PORT_IMGO,
  RAW_PORT_RRZO,
  RAW_PORT_MFBO,
  RAW_PORT_UNDEFINED = -1,
  // do NOT show port name
  RAW_PORT_NULL = -2,
};

enum YUV_PORT {
  YUV_PORT_IMG2O,
  YUV_PORT_IMG3O,
  YUV_PORT_WROTO,
  YUV_PORT_WDMAO,
  YUV_PORT_UNDEFINED = -1,
  // do NOT show port name
  YUV_PORT_NULL = -2,
};

struct VISIBILITY_PUBLIC FILE_DUMP_NAMING_HINT {
  FILE_DUMP_NAMING_HINT();

  // from IMetadata
  int UniqueKey;
  int FrameNo;
  int RequestNo;

  int ImgWidth;
  int ImgHeight;
  // image buffer size

  int ImgFormat;
  // mtkcam/def/ImageFormat.h (EImageFormat)

  // set directly
  int IspProfile;

  // Sensor
  int SensorFormatOrder;
  // mtkcam/drv/IHalSensor.h
  // - SENSOR_FORMAT_ORDER_RAW_B = 0x0,
  // - SENSOR_FORMAT_ORDER_RAW_Gb,
  // - SENSOR_FORMAT_ORDER_RAW_Gr,
  // - SENSOR_FORMAT_ORDER_RAW_R,

  int SensorType;
  int SensorOpenId;

  int SensorDev;
  // mtkcam/drv/IHalSensor.h
  // - SENSOR_DEV_MAIN
  // - SENSOR_DEV_SUB
  // - SENSOR_DEV_MAIN_2
  // - SENSOR_DEV_SUB_2

  int EvValue;

  // additional string for all getFilename
  char additStr[32];
};

bool VISIBILITY_PUBLIC extract(FILE_DUMP_NAMING_HINT* pHint,
                               const IMetadata* pMetadata);
bool VISIBILITY_PUBLIC extract(FILE_DUMP_NAMING_HINT* pHint,
                               const IImageBuffer* pImgBuf);
bool VISIBILITY_PUBLIC extract_by_SensorOpenId(FILE_DUMP_NAMING_HINT* pHint,
                                               int openId);
bool VISIBILITY_PUBLIC extract_by_SensorDev(FILE_DUMP_NAMING_HINT* pHint,
                                            int sensorDev);

// P2-like node dump
void VISIBILITY_PUBLIC genFileName_RAW(char* pFilename,
                                       int nFilename,
                                       const FILE_DUMP_NAMING_HINT* pHint,
                                       RAW_PORT port,
                                       const char* pUserString = NULL);

void VISIBILITY_PUBLIC genFileName_LCSO(char* pFilename,
                                        int nFilename,
                                        const FILE_DUMP_NAMING_HINT* pHint,
                                        const char* pUserString = NULL);

void VISIBILITY_PUBLIC genFileName_YUV(char* pFilename,
                                       int nFilename,
                                       const FILE_DUMP_NAMING_HINT* pHint,
                                       YUV_PORT port,
                                       const char* pUserString = NULL);

// 3A dump
void VISIBILITY_PUBLIC genFileName_HW_AAO(char* pFilename,
                                          int nFilename,
                                          const FILE_DUMP_NAMING_HINT* pHint,
                                          const char* pUserString = NULL);

void VISIBILITY_PUBLIC genFileName_TUNING(char* pFilename,
                                          int nFilename,
                                          const FILE_DUMP_NAMING_HINT* pHint,
                                          const char* pUserString = NULL);

void VISIBILITY_PUBLIC genFileName_Reg(char* pFilename,
                                       int nFilename,
                                       const FILE_DUMP_NAMING_HINT* pHint,
                                       const char* pUserString = NULL);

void VISIBILITY_PUBLIC genFileName_LSC(char* pFilename,
                                       int nFilename,
                                       const FILE_DUMP_NAMING_HINT* pHint,
                                       const char* pUserString = NULL);

void VISIBILITY_PUBLIC genFileName_LSC2(char* pFilename,
                                        int nFilename,
                                        const FILE_DUMP_NAMING_HINT* pHint,
                                        const char* pUserString = NULL);

// Jpeg dump
void VISIBILITY_PUBLIC genFileName_JPG(char* pFilename,
                                       int nFilename,
                                       const FILE_DUMP_NAMING_HINT* pHint,
                                       const char* pUserString = NULL);

void genFileName_VSDOF_BUFFER(char* pFilename,
                              int nFilename,
                              const FILE_DUMP_NAMING_HINT* pHint,
                              const char* pUserString);

}  // namespace TuningUtils
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_TUNINGUTILS_FILEDUMPNAMINGRULE_H_
