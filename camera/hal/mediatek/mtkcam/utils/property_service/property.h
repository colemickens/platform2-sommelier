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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_PROPERTY_SERVICE_PROPERTY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_PROPERTY_SERVICE_PROPERTY_H_

#define PROPERTY_VALUE_MAX 92
#define PROPERTY_KEY_MAX 64

// #define VERBOSE_DEBUG

struct prop_entry {
  char key[PROPERTY_KEY_MAX];
  char value[PROPERTY_VALUE_MAX];
  int valid;
};

#define POOL_SIZE 100

void update_property(const char* prop_name, char* prop_value);
int fetch_property(const char* prop_name, char* prop_value);

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_PROPERTY_SERVICE_PROPERTY_H_
