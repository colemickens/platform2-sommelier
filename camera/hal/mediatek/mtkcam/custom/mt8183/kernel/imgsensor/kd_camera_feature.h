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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_CAMERA_FEATURE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_CAMERA_FEATURE_H_

#ifndef FTYPE_ENUM
#define FTYPE_ENUM(_enums...) _enums
#endif /* FTYPE_ENUM */

#ifndef FID_TO_TYPE_ENUM
#define FID_TO_TYPE_ENUM(_fid, _enums) \
  typedef enum { _enums /*, OVER_NUM_OF_##_fid*/ }
#endif /* FID_TO_TYPE_ENUM */

#include <linux/string.h>

#include <kd_camera_feature_enum.h>
#include <kd_camera_feature_id.h>

enum IMGSENSOR_SENSOR_IDX {
  IMGSENSOR_SENSOR_IDX_MIN_NUM = 0,
  IMGSENSOR_SENSOR_IDX_MAIN = IMGSENSOR_SENSOR_IDX_MIN_NUM,
  IMGSENSOR_SENSOR_IDX_SUB,
  IMGSENSOR_SENSOR_IDX_MAIN2,
  IMGSENSOR_SENSOR_IDX_SUB2,
  IMGSENSOR_SENSOR_IDX_MAX_NUM,
  IMGSENSOR_SENSOR_IDX_NONE,
};

typedef enum {
  DUAL_CAMERA_NONE_SENSOR = 0,
  DUAL_CAMERA_MAIN_SENSOR = 1,
  DUAL_CAMERA_SUB_SENSOR = 2,
  DUAL_CAMERA_MAIN_2_SENSOR = 4,
  DUAL_CAMERA_SUB_2_SENSOR = 8,
  DUAL_CAMERA_SENSOR_MAX,
  /* for backward compatible */
  DUAL_CAMERA_MAIN_SECOND_SENSOR = DUAL_CAMERA_MAIN_2_SENSOR,
} CAMERA_DUAL_CAMERA_SENSOR_ENUM;

#define IMGSENSOR_SENSOR_DUAL2IDX(idx) ((ffs(idx) - 1))
#define IMGSENSOR_SENSOR_IDX2DUAL(idx) (1 << (idx))

#define IMGSENSOR_SENSOR_IDX_MAP(idx)                                 \
  ((idx) > DUAL_CAMERA_NONE_SENSOR && (idx) < DUAL_CAMERA_SENSOR_MAX) \
      ? (enum IMGSENSOR_SENSOR_IDX)IMGSENSOR_SENSOR_DUAL2IDX(idx)     \
      : IMGSENSOR_SENSOR_IDX_NONE

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_MT8183_KERNEL_IMGSENSOR_KD_CAMERA_FEATURE_H_
