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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_IMAGEFORMAT_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_IMAGEFORMAT_H_
//
#include <system/graphics.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *
 * @enum
 * @brief Transformation definitions.
 *        ROT_90 is applied CLOCKWISE and AFTER TRANSFORM_FLIP_{H|V}.
 *
 ******************************************************************************/
enum {
  /* do not rotate or flip image */
  eTransform_None = 0x00,

  /* flip source image horizontally (around the vertical axis) */
  eTransform_FLIP_H = 0x01,

  /* flip source image vertically (around the horizontal axis)*/
  eTransform_FLIP_V = 0x02,

  /* rotate source image 90 degrees clockwise */
  eTransform_ROT_90 = 0x04,

  /* rotate source image 180 degrees */
  eTransform_ROT_180 = 0x03,

  /* rotate source image 270 degrees clockwise */
  eTransform_ROT_270 = 0x07,
};

/******************************************************************************
 *
 * @enum EImageFormat
 * @brief Image format Enumeration.
 *
 ******************************************************************************/
enum EImageFormat {
  eImgFmt_IMPLEMENTATION_DEFINED = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,

  eImgFmt_RAW16 = HAL_PIXEL_FORMAT_RAW16,           /*!< raw 16-bit 1 plane */
  eImgFmt_RAW_OPAQUE = HAL_PIXEL_FORMAT_RAW_OPAQUE, /*!< raw 1 plane */

  /*
   * This format is used to carry task-specific data which does not have a
   * standard image structure. The details of the format are left to the two
   * endpoints.
   *
   * Buffers of this format must have a height of 1, and width equal to their
   * size in bytes.
   */
  eImgFmt_BLOB = HAL_PIXEL_FORMAT_BLOB,

  eImgFmt_RGBA8888 =
      HAL_PIXEL_FORMAT_RGBA_8888, /*!< RGBA (32-bit; LSB:R, MSB:A), 1 plane */
  eImgFmt_RGBX8888 =
      HAL_PIXEL_FORMAT_RGBX_8888, /*!< RGBX (32-bit; LSB:R, MSB:X), 1 plane */
  eImgFmt_RGB888 =
      HAL_PIXEL_FORMAT_RGB_888, /*!< RGB 888 (24-bit), 1 plane (RGB) */
  eImgFmt_RGB565 = HAL_PIXEL_FORMAT_RGB_565, /*!< RGB 565 (16-bit), 1 plane */
  eImgFmt_BGRA8888 =
      HAL_PIXEL_FORMAT_BGRA_8888, /*!< BGRA (32-bit; LSB:B, MSB:A), 1 plane */

  eImgFmt_YUY2 =
      HAL_PIXEL_FORMAT_YCbCr_422_I, /*!< 422 format, 1 plane (YUYV) */

  eImgFmt_NV16 =
      HAL_PIXEL_FORMAT_YCbCr_422_SP, /*!< 422 format, 2 plane (Y),(UV) */
  eImgFmt_NV21 =
      HAL_PIXEL_FORMAT_YCrCb_420_SP, /*!< 420 format, 2 plane (Y),(VU) */

  eImgFmt_YV12 = HAL_PIXEL_FORMAT_YV12, /*!< 420 format, 3 plane (Y),(V),(U) */

  eImgFmt_Y8 = HAL_PIXEL_FORMAT_Y8, /*!<  8-bit Y plane */
  eImgFmt_Y800 = eImgFmt_Y8, /*!< deprecated; Replace it with eImgFmt_Y8 */
  eImgFmt_Y16 = HAL_PIXEL_FORMAT_Y16, /*!< 16-bit Y plane */

  eImgFmt_CAMERA_OPAQUE =
      eImgFmt_RAW_OPAQUE, /*!< Opaque format, RAW10 + Metadata */

  /**************************************************************************
   * 0x2000 - 0x2FFF
   *
   * This range is reserved for pixel formats that are specific to the HAL
   *implementation.
   **************************************************************************/
  eImgFmt_UNKNOWN = 0x0000,              /*!< unknow */
  eImgFmt_VENDOR_DEFINED_START = 0x2000, /*!< vendor definition start */

  /* please add YUV format after eImgFmt_YUV_START */
  eImgFmt_YUV_START = eImgFmt_VENDOR_DEFINED_START,
  eImgFmt_YVYU = eImgFmt_YUV_START, /*!< 422 format, 1 plane (YVYU) */
  eImgFmt_UYVY,                     /*!< 422 format, 1 plane (UYVY) */
  eImgFmt_VYUY,                     /*!< 422 format, 1 plane (VYUY) */

  eImgFmt_NV61,     /*!< 422 format, 2 plane (Y),(VU) */
  eImgFmt_NV12,     /*!< 420 format, 2 plane (Y),(UV) */
  eImgFmt_NV12_BLK, /*!< 420 format block mode, 2 plane (Y),(UV) */
  eImgFmt_NV21_BLK, /*!< 420 format block mode, 2 plane (Y),(VU) */

  eImgFmt_YV16, /*!< 422 format, 3 plane (Y),(V),(U) */
  eImgFmt_I420, /*!< 420 format, 3 plane (Y),(U),(V) */
  eImgFmt_I422, /*!< 422 format, 3 plane (Y),(U),(V) */

  /* please add RGB format after eImgFmt_RGB_START */
  eImgFmt_RGB_START = 0x2100,
  eImgFmt_ARGB8888 =
      eImgFmt_RGB_START, /*!< ARGB (32-bit; LSB:A, MSB:B), 1 plane */
  eImgFmt_ARGB888 =
      eImgFmt_ARGB8888, /*!< deprecated; Replace it with eImgFmt_ARGB8888 */
  eImgFmt_RGB48,        /*!< RGB 48(16x3, 48-bit; LSB:R, MSB:B), 1 plane */

  /* please add RAW format after eImgFmt_RAW_START */
  eImgFmt_RAW_START = 0x2200,
  eImgFmt_BAYER8 = eImgFmt_RAW_START, /*!< Bayer format, 8-bit */
  eImgFmt_BAYER10,                    /*!< Bayer format, 10-bit */
  eImgFmt_BAYER12,                    /*!< Bayer format, 12-bit */
  eImgFmt_BAYER14,                    /*!< Bayer format, 14-bit */

  eImgFmt_FG_BAYER8,  /*!< Full-G (8-bit) */
  eImgFmt_FG_BAYER10, /*!< Full-G (10-bit) */
  eImgFmt_FG_BAYER12, /*!< Full-G (12-bit) */
  eImgFmt_FG_BAYER14, /*!< Full-G (14-bit) */

  /*IMGO/RRZO UF format*/
  eImgFmt_UFO_BAYER8,  /*!< UFO (8-bit) */
  eImgFmt_UFO_BAYER10, /*!< UFO (10-bit) */
  eImgFmt_UFO_BAYER12, /*!< UFO (12-bit) */
  eImgFmt_UFO_BAYER14, /*!< UFO (14-bit) */

  eImgFmt_UFEO_BAYER8,                         /*only for 6799 rrzo UF format*/
  eImgFmt_UFO_FG_BAYER8 = eImgFmt_UFEO_BAYER8, /*!< UFO Full-G(8-bit) */
  eImgFmt_UFEO_BAYER10,                        /*only for 6799 rrzo UF format*/
  eImgFmt_UFO_FG_BAYER10 = eImgFmt_UFEO_BAYER10, /*!< UFO Full-G(10-bit) */
  eImgFmt_UFEO_BAYER12, /*only for 6799 rrzo UF format*/
  eImgFmt_UFO_FG_BAYER12 = eImgFmt_UFEO_BAYER12, /*!< UFO Full-G(12-bit) */
  eImgFmt_UFEO_BAYER14, /*only for 6799 rrzo UF format*/
  eImgFmt_UFO_FG_BAYER14 = eImgFmt_UFEO_BAYER14, /*!< UFO Full-G(14-bit) */

  eImgFmt_UFO_FG, /*!< UFO (Full-G) */

  eImgFmt_BAYER10_MIPI, /*!< Bayer format, 10-bit (MIPI) */

  eImgFmt_BAYER8_UNPAK,  /*!< Bayer format,unpaked 16-bit */
  eImgFmt_BAYER10_UNPAK, /*!< Bayer format,unpaked 16-bit */
  eImgFmt_BAYER12_UNPAK, /*!< Bayer format,unpaked 16-bit */
  eImgFmt_BAYER14_UNPAK, /*!< Bayer format,unpaked 16-bit */

  eImgFmt_WARP_2PLANE, /*!< Warp format, 32-bit, 2 plane (X), (Y) */
  eImgFmt_WARP_3PLANE, /*!< Warp format, 32-bit, 3 plane (X), (Y), (Z) */

  /* please add BLOB format after eImgFmt_BLOB_START */
  eImgFmt_BLOB_START = 0x2300,
  eImgFmt_JPEG = eImgFmt_BLOB_START, /*!< JPEG format */
  eImgFmt_JPG_I420, /*!< JPEG 420 format, 3 plane (Y),(U),(V) */
  eImgFmt_JPG_I422, /*!< JPEG 422 format, 3 plane (Y),(U),(V) */

  /**************************************************************************
   * This section is used for non-image-format buffer.
   **************************************************************************/
  /* please add STATIC format after eImgFmt_STA_START */
  eImgFmt_STA_START = 0x2400,
  eImgFmt_STA_BYTE = eImgFmt_STA_START, /*!< statistic format, 8-bit */
  eImgFmt_STA_2BYTE                     /*!< statistic format, 16-bit */
};

enum EImageBayerFormat {
  // Be ware the order of EImageBayerFormat should be the same with pixel order
  // in IHalSensor.h
  // Otherwise it will result in mapping fail at p1 wrapper. order: B->GB->GR->R
  eImgFmt_BAYER8_BGGR = (eImgFmt_BAYER8 << 2),
  eImgFmt_BAYER8_GBRG,
  eImgFmt_BAYER8_GRBG,
  eImgFmt_BAYER8_RGGB,
  eImgFmt_BAYER10_BGGR = (eImgFmt_BAYER10 << 2),
  eImgFmt_BAYER10_GBRG,
  eImgFmt_BAYER10_GRBG,
  eImgFmt_BAYER10_RGGB,
  eImgFmt_BAYER12_BGGR = (eImgFmt_BAYER12 << 2),
  eImgFmt_BAYER12_GBRG,
  eImgFmt_BAYER12_GRBG,
  eImgFmt_BAYER12_RGGB,
  eImgFmt_BAYER14_BGGR = (eImgFmt_BAYER14 << 2),
  eImgFmt_BAYER14_GBRG,
  eImgFmt_BAYER14_GRBG,
  eImgFmt_BAYER14_RGGB,
  eImgFmt_FG_BAYER8_BGGR = (eImgFmt_FG_BAYER8 << 2),
  eImgFmt_FG_BAYER8_GBRG,
  eImgFmt_FG_BAYER8_GRBG,
  eImgFmt_FG_BAYER8_RGGB,
  eImgFmt_FG_BAYER10_BGGR = (eImgFmt_FG_BAYER10 << 2),
  eImgFmt_FG_BAYER10_GBRG,
  eImgFmt_FG_BAYER10_GRBG,
  eImgFmt_FG_BAYER10_RGGB,
  eImgFmt_FG_BAYER12_BGGR = (eImgFmt_FG_BAYER12 << 2),
  eImgFmt_FG_BAYER12_GBRG,
  eImgFmt_FG_BAYER12_GRBG,
  eImgFmt_FG_BAYER12_RGGB,
  eImgFmt_FG_BAYER14_BGGR = (eImgFmt_FG_BAYER14 << 2),
  eImgFmt_FG_BAYER14_GBRG,
  eImgFmt_FG_BAYER14_GRBG,
  eImgFmt_FG_BAYER14_RGGB,
};

/**
 * @brief transform bayer format to bayer format with order, if the foramt is
 * not bayer then do nothing
 * @param img_fmt is the imaage foramt such as eImgFmt_BAYER8, eImgFmt_BAYER10,
 * eImgFmt_YVYU, ...
 * @param sensor_bayer_order is the order of bayer pattern which is defined at
 * IHalsensor.h
 */
inline int bayerOrderTransform(int img_fmt, int sensor_bayer_order) {
  if (img_fmt >= eImgFmt_BAYER8 && img_fmt <= eImgFmt_FG_BAYER14)
    return (img_fmt << 2) + sensor_bayer_order;
  else
    return img_fmt;
}
/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_IMAGEFORMAT_H_
