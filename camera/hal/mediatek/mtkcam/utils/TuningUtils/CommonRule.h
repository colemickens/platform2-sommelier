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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_TUNINGUTILS_COMMONRULE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_TUNINGUTILS_COMMONRULE_H_

#include <isp_tuning/isp_tuning.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <property_service/property.h>
#include <property_service/property_lib.h>

#define IMAGE_FORMAT_TO_BITS(e)                                                          \
  (e == eImgFmt_BAYER10)                                                                 \
      ? "10"                                                                             \
      : (e == eImgFmt_BAYER12)                                                           \
            ? "12"                                                                       \
            : (e == eImgFmt_BAYER14)                                                     \
                  ? "14"                                                                 \
                  : (e == eImgFmt_BAYER10_UNPAK)                                         \
                        ? "10"                                                           \
                        : (e == eImgFmt_BAYER12_UNPAK)                                   \
                              ? "12"                                                     \
                              : (e == eImgFmt_BAYER14_UNPAK)                             \
                                    ? "14"                                               \
                                    : (e == eImgFmt_FG_BAYER10)                          \
                                          ? "10"                                         \
                                          : (e == eImgFmt_FG_BAYER12)                    \
                                                ? "12"                                   \
                                                : (e == eImgFmt_FG_BAYER14)              \
                                                      ? "14"                             \
                                                      : (e == eImgFmt_YV12)              \
                                                            ? "yv12"                     \
                                                            : (e ==                      \
                                                               eImgFmt_NV21)             \
                                                                  ? "nv21"               \
                                                                  : (e ==                \
                                                                     eImgFmt_YUY2)       \
                                                                        ? "yu"           \
                                                                          "y2"           \
                                                                        : (e ==          \
                                                                           eImgFmt_I420) \
                                                                              ? "i420"   \
                                                                              : "imgfmt"

#define IMAGE_FORMAT_TO_FILE_EXT(e)                                                      \
  (e == eImgFmt_BAYER10)                                                                 \
      ? "packed_word"                                                                    \
      : (e == eImgFmt_BAYER12)                                                           \
            ? "packed_word"                                                              \
            : (e == eImgFmt_BAYER14)                                                     \
                  ? "packed_word"                                                        \
                  : (e == eImgFmt_BAYER10_UNPAK)                                         \
                        ? "raw"                                                          \
                        : (e == eImgFmt_BAYER12_UNPAK)                                   \
                              ? "raw"                                                    \
                              : (e == eImgFmt_BAYER14_UNPAK)                             \
                                    ? "raw"                                              \
                                    : (e == eImgFmt_FG_BAYER10)                          \
                                          ? "packed_word"                                \
                                          : (e == eImgFmt_FG_BAYER12)                    \
                                                ? "packed_word"                          \
                                                : (e == eImgFmt_FG_BAYER14)              \
                                                      ? "packed_word"                    \
                                                      : (e == eImgFmt_YV12)              \
                                                            ? "yv12"                     \
                                                            : (e ==                      \
                                                               eImgFmt_NV21)             \
                                                                  ? "nv21"               \
                                                                  : (e ==                \
                                                                     eImgFmt_YUY2)       \
                                                                        ? "yu"           \
                                                                          "y2"           \
                                                                        : (e ==          \
                                                                           eImgFmt_I420) \
                                                                              ? "i420"   \
                                                                              : "unknown"

#define SENSOR_FORMAT_TO_STRING(e)              \
  (e == SENSOR_FORMAT_ORDER_RAW_B)              \
      ? "0"                                     \
      : (e == SENSOR_FORMAT_ORDER_RAW_Gb)       \
            ? "1"                               \
            : (e == SENSOR_FORMAT_ORDER_RAW_Gr) \
                  ? "2"                         \
                  : (e == SENSOR_FORMAT_ORDER_RAW_R) ? "3" : "senfmt"

#define SENSOR_DEV_TO_STRING(e)        \
  (e == SENSOR_DEV_MAIN)               \
      ? "main"                         \
      : (e == SENSOR_DEV_SUB)          \
            ? "sub"                    \
            : (e == SENSOR_DEV_MAIN_2) \
                  ? "main2"            \
                  : (e == SENSOR_DEV_SUB_2) ? "sub2" : "sensor"

#define RAW_PORT_TO_STRING(e) \
  (e == RAW_PORT_IMGO) ? "imgo" : (e == RAW_PORT_RRZO) ? "rrzo" : "rawport"

#define YUV_PORT_TO_STRING(e)           \
  (e == YUV_PORT_IMG2O)                 \
      ? "img2o"                         \
      : (e == YUV_PORT_IMG3O) ? "img3o" \
                              : (e == YUV_PORT_WROTO) ? "wroto" : "yuvport"

#define LSC2_TBL_TYPE_TO_STRING(e) \
  (e == LSC2_TBL_TYPE_SDBLK)       \
      ? "sdblk"                    \
      : (e == LSC2_TBL_TYPE_HWTBL) ? "hwtbl" : "shading"

namespace NSCam {
namespace TuningUtils {

int MakePrefix(char* Filename,
               int nFilename,
               int UniqueKey,
               int FrameNo,
               int RequestNo,
               int EvValue = 0);
const char* getIspProfileName(NSIspTuning::EIspProfile_T IspProfile);

}  // namespace TuningUtils
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_TUNINGUTILS_COMMONRULE_H_
