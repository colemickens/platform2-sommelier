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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_FEATURE_CAMERA_FEATURE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_FEATURE_CAMERA_FEATURE_H_

#include "camera_feature/camera_feature_id.h"
#include "camera_feature/camera_feature_utility.h"
namespace NSFeature {
/* Include kd_camera_feature_enum anyway */
#ifdef _KD_CAMERA_FEATURE_ENUM_H_

#undef _KD_CAMERA_FEATURE_ENUM_H_
#include <kd_camera_feature_enum.h>

#else

#include <kd_camera_feature_enum.h>
#undef _KD_CAMERA_FEATURE_ENUM_H_

#endif

/* Unset macro definition */
#undef FTYPE_ENUM
#undef FID_TO_TYPE_ENUM
};  //  namespace NSFeature

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_CUSTOM_COMMON_HAL_INC_CAMERA_FEATURE_CAMERA_FEATURE_H_
