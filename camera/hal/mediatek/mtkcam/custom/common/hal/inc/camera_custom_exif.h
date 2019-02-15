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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_CUSTOM_EXIF_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_CUSTOM_EXIF_H_
//
#include "inc/camera_custom_types.h"

//
namespace NSCamCustom {
/*******************************************************************************
 * Custom EXIF: Imgsensor-related.
 ******************************************************************************/
typedef struct SensorExifInfo_S {
  MUINT32 uFLengthNum;
  MUINT32 uFLengthDenom;
} SensorExifInfo_T;

SensorExifInfo_T const& getParamSensorExif() {
  static SensorExifInfo_T inst = {
      .uFLengthNum = 35,    // Numerator of Focal Length. Default is 35.
      .uFLengthDenom = 10,  // Denominator of Focal Length, it should not be 0.
                            // Default is 10.
  };
  return inst;
}

/*******************************************************************************
 * Custom EXIF
 ******************************************************************************/
// #define EN_CUSTOM_EXIF_INFO
#define SET_EXIF_TAG_STRING(tag, str)                                  \
  if (strlen((const char*)str) <= 32) {                                \
    snprintf(pexifApp1Info->tag, strlen((const char*)str), "%s", str); \
  }

typedef struct customExifInfo_s {
  unsigned char strMake[32];
  unsigned char strModel[32];
  unsigned char strSoftware[32];
} customExifInfo_t;

#ifdef EN_CUSTOM_EXIF_INFO
#define CUSTOM_EXIF_STRING_MAKE "custom make"
#define CUSTOM_EXIF_STRING_MODEL "custom model"
#define CUSTOM_EXIF_STRING_SOFTWARE "custom software"
MINT32 custom_SetExif(void** ppCustomExifTag) {
  static customExifInfo_t exifTag = {CUSTOM_EXIF_STRING_MAKE,
                                     CUSTOM_EXIF_STRING_MODEL,
                                     CUSTOM_EXIF_STRING_SOFTWARE};
  if (0 != ppCustomExifTag) {
    *ppCustomExifTag = reinterpret_cast<void*>(&exifTag);
  }
  return 0;
}
#else
MINT32 custom_SetExif(void** /*ppCustomExifTag*/) {
  return -1;
}
#endif

/*******************************************************************************
 * Custom EXIF: Exposure Program
 ******************************************************************************/
typedef struct customExif_s {
  MBOOL bEnCustom;
  MUINT32 u4ExpProgram;
} customExif_t;

customExif_t const& getCustomExif() {
  static customExif_t inst = {
      .bEnCustom = false,  // default value: false.
      .u4ExpProgram = 0,   // default value: 0.    '0' means not defined, '1'
                           // manual control, '2' program normal
  };
  return inst;
}

};      // namespace NSCamCustom
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_CUSTOM_EXIF_H_
