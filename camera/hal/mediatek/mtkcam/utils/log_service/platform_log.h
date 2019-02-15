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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_LOG_SERVICE_PLATFORM_LOG_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_LOG_SERVICE_PLATFORM_LOG_H_

#ifndef VISIBILITY_PUBLIC
#define VISIBILITY_PUBLIC __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

int VISIBILITY_PUBLIC platform_log_print(int prio,
                                         const char* tag,
                                         const char* fmt,
                                         ...);

#ifdef __cplusplus
}
#endif

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_LOG_SERVICE_PLATFORM_LOG_H_
