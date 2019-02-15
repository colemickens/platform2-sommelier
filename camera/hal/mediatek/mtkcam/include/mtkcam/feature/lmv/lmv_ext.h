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

/**
 * @file lmv_ext.h
 *
 * LMV extension Header File
 */

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_LMV_LMV_EXT_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_LMV_LMV_EXT_H_

#define LMV_MAX_GMV_32 (32)
#define LMV_MAX_GMV_64 (64)
#define LMV_MAX_GMV_DEFAULT LMV_MAX_GMV_32
#define LMV_GMV_VALUE_TO_PIXEL_UNIT \
  (256)  // The unit of Gmv is 256x 'pixel', so /256 to change unit to 'pixel'.

/**
 *  Data type in MTK_EIS_REGION is all MINT32
 */
typedef enum {
  LMV_REGION_INDEX_XINT = 0,
  LMV_REGION_INDEX_XFLOAT,
  LMV_REGION_INDEX_YINT,
  LMV_REGION_INDEX_YFLOAT,
  /**
   *  Resolution here is describing that the ROI of EIS, a central cropping
   *  should be applied follows this resolution.
   */
  LMV_REGION_INDEX_WIDTH,   // width that valid by LMV
  LMV_REGION_INDEX_HEIGHT,  // height that valid by LMV
  LMV_REGION_INDEX_MV2CENTERX,
  LMV_REGION_INDEX_MV2CENTERY,
  LMV_REGION_INDEX_ISFROMRZ,
  LMV_REGION_INDEX_GMVX,
  LMV_REGION_INDEX_GMVY,  // index 10
  LMV_REGION_INDEX_CONFX,
  LMV_REGION_INDEX_CONFY,
  LMV_REGION_INDEX_EXPTIME,
  LMV_REGION_INDEX_HWTS,     // highword timestamp (bit[32:63])
  LMV_REGION_INDEX_LWTS,     // lowword timestamp  (bit[0:31])
  LMV_REGION_INDEX_MAX_GMV,  // max search range
  LMV_REGION_INDEX_ISFRONTBIN,
  /* for indicating to size only */
  LMV_REGION_INDEX_SIZE,
} LMV_REGION_INDEX;

struct LMVData {
  LMVData() {}

  LMVData(MINT32 _cmv_x_int,
          MINT32 _cmv_x_float,
          MINT32 _cmv_y_int,
          MINT32 _cmv_y_float,
          MINT32 _width,
          MINT32 _height,
          MINT32 _cmv_x_center,
          MINT32 _cmv_y_center,
          MINT32 _isFromRRZ,
          MINT32 _gmv_x,
          MINT32 _gmv_y,
          MINT32 _conf_x,
          MINT32 _conf_y,
          MINT32 _expTime,
          MINT32 _hwTs,
          MINT32 _lwTs,
          MINT32 _maxGMV,
          MINT32 _isFrontBin)
      : cmv_x_int(_cmv_x_int),
        cmv_x_float(_cmv_x_float),
        cmv_y_int(_cmv_y_int),
        cmv_y_float(_cmv_y_float),
        width(_width),
        height(_height),
        cmv_x_center(_cmv_x_center),
        cmv_y_center(_cmv_y_center),
        isFromRRZ(_isFromRRZ),
        gmv_x(_gmv_x),
        gmv_y(_gmv_y),
        conf_x(_conf_x),
        conf_y(_conf_y),
        expTime(_expTime),
        hwTs(_hwTs),
        lwTs(_lwTs),
        maxGMV(_maxGMV),
        isFrontBin(_isFrontBin) {}

  MINT32 cmv_x_int = 0;
  MINT32 cmv_x_float = 0;
  MINT32 cmv_y_int = 0;
  MINT32 cmv_y_float = 0;
  MINT32 width = 0;
  MINT32 height = 0;
  MINT32 cmv_x_center = 0;
  MINT32 cmv_y_center = 0;
  MINT32 isFromRRZ = 0;
  MINT32 gmv_x = 0;
  MINT32 gmv_y = 0;
  MINT32 conf_x = 0;
  MINT32 conf_y = 0;
  MINT32 expTime = 0;
  MINT32 hwTs = 0;
  MINT32 lwTs = 0;
  MINT32 maxGMV = 0;
  MINT32 isFrontBin = 0;
};

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_FEATURE_LMV_LMV_EXT_H_
