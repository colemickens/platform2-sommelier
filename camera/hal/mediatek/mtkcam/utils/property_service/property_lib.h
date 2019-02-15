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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_PROPERTY_SERVICE_PROPERTY_LIB_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_PROPERTY_SERVICE_PROPERTY_LIB_H_

#include <mtkcam/def/common.h>
#include <mtkcam/utils/property_service/property.h>

#ifdef __cplusplus
extern "C" {
#endif
int VISIBILITY_PUBLIC property_get(const char* key,
                                   char* value,
                                   const char* default_value);
int VISIBILITY_PUBLIC property_get_int32(const char* key, int default_value);
int VISIBILITY_PUBLIC property_get_int32_remote(const char* key,
                                                int default_value);
int VISIBILITY_PUBLIC property_set(const char* key, char* value);
#ifdef __cplusplus
}
#endif

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_PROPERTY_SERVICE_PROPERTY_LIB_H_
