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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_EXIF_IBASECAMEXIF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_EXIF_IBASECAMEXIF_H_

#include <mtkcam/def/BuiltinTypes.h>
#include <string.h>

/*******************************************************************************
 *
 ******************************************************************************/
#define DBG_EXIF_SIZE (0xFFFF * 4)
#define REFOCUS_EXIF_SIZE (0xFFFF * 5)  // for Image Refocus jpeg
#define STEREO_EXIF_SIZE (0xFFFF * 8)   // for JPS

/*******************************************************************************
 * Camera EXIF Command
 ******************************************************************************/
typedef enum DEBUG_EXIF_CMD_S {
  CMD_REGISTER = 0x1001,
  CMD_SET_DBG_EXIF
} DEBUG_EXIF_CMD_E;

/*******************************************************************************
 * V3: standard exif information
 ******************************************************************************/
struct ExifParams {
  MUINT32 u4ImageWidth;   // Image width
  MUINT32 u4ImageHeight;  // Image height
  //
  MUINT32 u4FNumber;           // Format: F2.8 = 28
  MUINT32 u4FocalLength;       // Format: FL 3.5 = 350
  MUINT32 u4FocalLength35mm;   // Format: FL35mm 28 = 28
  MUINT32 u4AWBMode;           // White balance mode
  MUINT32 u4LightSource;       // Light Source mode
  MUINT32 u4ExpProgram;        // Exposure Program
  MUINT32 u4SceneCapType;      // Scene Capture Type
  MUINT32 u4FlashLightTimeus;  // Strobe on/off
  MUINT32 u4AEMeterMode;       // Exposure metering mode
  MINT32 i4AEExpBias;          // Exposure index*10
  MUINT32 u4CapExposureTime;   //
  MUINT32 u4AEISOSpeed;        // AE ISO value
  //
  MUINT32 u4GpsIsOn;
  MUINT32 u4GPSAltitude;
  MUINT8 uGPSLatitude[32];
  MUINT8 uGPSLongitude[32];
  MUINT8 uGPSTimeStamp[32];
  MUINT8 uGPSProcessingMethod[64];  // (values of "GPS", "CELLID", "WLAN" or
                                    // "MANUAL" by the EXIF spec.)
  //
  MUINT32 u4Orientation;  // 0, 90, 180, 270
  MUINT32 u4ZoomRatio;  // Digital zoom ratio (x100) For example, 100, 114, and
                        // 132 refer to 1.00, 1.14, and 1.32 respectively.
  //
  MUINT32 u4Facing;  // 1: front camera, 0: not front
  MUINT32 u4ICCIdx;
  //
 public:  ////    Operations.
  ExifParams() { ::memset(this, 0, sizeof(ExifParams)); }
};

/*******************************************************************************
 *
 ******************************************************************************/

enum ECapTypeId {
  eCapTypeId_Standard = 0,
  eCapTypeId_Landscape = 1,
  eCapTypeId_Portrait = 2,
  eCapTypeId_Night = 3
};

enum EExpProgramId {
  eExpProgramId_NotDefined = 0,
  eExpProgramId_Manual = 1,
  eExpProgramId_Normal = 2,
  eExpProgramId_Portrait = 7,
  eExpProgramId_Landscape = 8
};

enum ELightSourceId {
  eLightSourceId_Daylight = 1,
  eLightSourceId_Fluorescent = 2,
  eLightSourceId_Tungsten = 3,
  eLightSourceId_Cloudy = 10,
  eLightSourceId_Shade = 11,
  eLightSourceId_Other = 255
};

enum EMeteringModeId {
  eMeteringMode_Average = 1,
  eMeteringMode_Center = 2,
  eMeteringMode_Spot = 3,
  eMeteringMode_Other = 255
};

/*******************************************************************************
 * (Basic) Camera Exif
 ******************************************************************************/
class IBaseCamExif {
 public:  ////    Interfaces.
  IBaseCamExif() {}
  virtual ~IBaseCamExif() {}

  //=============================================================
  //  Interfaces.
  //=============================================================

  /****************************************************************************
   *  sendCommand
   ****************************************************************************/
  virtual MBOOL sendCommand(MINT32 cmd,
                            MINT32 arg1 = 0,
                            MUINTPTR arg2 = 0,
                            MINT32 arg3 = 0) = 0;
};

#endif  //  CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_EXIF_IBASECAMEXIF_H_
