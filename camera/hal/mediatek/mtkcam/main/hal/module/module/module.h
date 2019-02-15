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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_MODULE_MODULE_MODULE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_MODULE_MODULE_MODULE_H_

/******************************************************************************
 *
 ******************************************************************************/
#include <mtkcam/main/hal/ICamDeviceManager.h>

static int open_device(hw_module_t const* module,
                       const char* name,
                       hw_device_t** device);

static hw_module_methods_t* get_module_methods();

static int get_number_of_cameras(void);

static int get_camera_info(int cameraId, camera_info* info);

static int set_callbacks(camera_module_callbacks_t const* callbacks);

static int open_legacy(const struct hw_module_t* module,
                       const char* id,
                       uint32_t halVersion,
                       struct hw_device_t** device);

static camera_module get_camera_module();

/******************************************************************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_MODULE_MODULE_MODULE_H_
