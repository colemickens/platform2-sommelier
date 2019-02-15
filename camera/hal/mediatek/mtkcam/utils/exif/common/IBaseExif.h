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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_IBASEEXIF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_IBASEEXIF_H_

#include <exif/common/exif_sdflags.h>
#include <mtkcam/def/BuiltinTypes.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************
 *
 ******************************************************************************/
typedef struct exifImageInfo_s {
  MUINTPTR bufAddr;
  unsigned int mainWidth;
  unsigned int mainHeight;
  unsigned int thumbSize;
} exifImageInfo_t;

typedef struct exifAPP1Info_s {
  unsigned int imgIndex;  // mtk definition: the index of continuous shot image.
  unsigned int groupID;   // mtk definition: group ID for continuous shot.
  unsigned int bestFocusH;  // mtk definition: focus value (H) for best shot.
  unsigned int bestFocusL;  // mtk definition: focus value (L) for best shot.
  unsigned int refocusPos;  // mtk definition: for image refocus. JPEG(main
                            // sensor) in left or right position.
  unsigned char strJpsFileName[32];  // mtk definition: for image refocus. JPS
                                     // file name for calculating depth map.
  unsigned int exposureTime[2];
  unsigned int fnumber[2];
  int exposureBiasValue[2];
  unsigned int focalLength[2];
  uint16_t focalLength35mm;
  uint16_t orientation;
  uint16_t exposureProgram;
  uint16_t isoSpeedRatings;
  uint16_t meteringMode;
  uint16_t flash;
  uint16_t whiteBalanceMode;
  uint16_t reserved;
  unsigned char strImageDescription[32];
  unsigned char strMake[32];
  unsigned char strModel[32];
  unsigned char strSoftware[32];
  unsigned char strDateTime[20];
  unsigned char strSubSecTime[4];
  unsigned char gpsLatitudeRef[2];
  unsigned char gpsLongitudeRef[2];
  unsigned char reserved02;
  unsigned int digitalZoomRatio[2];
  uint16_t sceneCaptureType;
  uint16_t lightSource;
  unsigned char strFlashPixVer[8];
  uint16_t exposureMode;
  uint16_t reserved03;
  int gpsIsOn;
  int gpsAltitude[2];
  int gpsLatitude[8];
  int gpsLongitude[8];
  int gpsTimeStamp[8];
  unsigned char gpsDateStamp[12];
  unsigned char gpsProcessingMethod[64];
} exifAPP1Info_t;

typedef struct exifAPP3Info_s {
  unsigned char identifier[8];
  unsigned char length[2];
  struct Comments {
    unsigned char size[2];
    unsigned char comment[16];
  } cmt;
} exifAPP3Info_t;

/****************************************************************************
 *  BaseExif Interface
 ****************************************************************************/
class IBaseExif {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Instantiation.
  virtual ~IBaseExif() {}
  IBaseExif() {}

 public:  ////                        Operations.
  virtual unsigned int exifApp1Make(exifImageInfo_t* pexifImgInfo,
                                    exifAPP1Info_t* pexifAPP1Info,
                                    unsigned int* pretSize) = 0;

  virtual unsigned int exifAppnMake(unsigned int appn,
                                    unsigned char* paddr,
                                    unsigned char* pdata,
                                    unsigned int dataSize,
                                    unsigned int* pretSize,
                                    unsigned int defaultSize = 0) = 0;

  virtual bool init(unsigned int const gpsEnFlag) = 0;

  virtual bool uninit() = 0;

  virtual size_t exifApp1SizeGet() const = 0;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_EXIF_COMMON_IBASEEXIF_H_
