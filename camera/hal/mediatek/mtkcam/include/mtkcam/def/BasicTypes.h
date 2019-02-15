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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_BASICTYPES_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_BASICTYPES_H_

#include <mtkcam/def/BuiltinTypes.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *  Rational
 ******************************************************************************/
struct MRational {
  MINT32 numerator;
  MINT32 denominator;
  //
  explicit MRational(MINT32 _numerator = 0, MINT32 _denominator = 1)
      : numerator(_numerator), denominator(_denominator) {}
};

/******************************************************************************
 *
 * @brief Sensor type enumeration.
 *
 ******************************************************************************/
namespace NSSensorType {
enum Type {
  eUNKNOWN = 0xFFFFFFFF, /*!< Unknown */
  eRAW = 0,              /*!< RAW */
  eYUV,                  /*!< YUV */
};
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_DEF_BASICTYPES_H_
