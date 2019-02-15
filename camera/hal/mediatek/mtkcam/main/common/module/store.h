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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_COMMON_MODULE_STORE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_COMMON_MODULE_STORE_H_

/******************************************************************************
 *
 ******************************************************************************/
#include <mtkcam/def/common.h>
#include <mtkcam/main/common/module.h>

/******************************************************************************
 *
 ******************************************************************************/
struct mtkcam_module_info {
  unsigned int module_id = 0;
  void* module_factory = nullptr;
  char const* register_name = nullptr;
};

void register_mtkcam_module(mtkcam_module_info const*,
                            NSCam::Int2Type<MTKCAM_MODULE_GROUP_ID>);

/******************************************************************************
 *
 ******************************************************************************/
#define REGISTER_MTKCAM_MODULE(_module_id_, _factory_)                        \
  namespace {                                                                 \
  struct auto_register_mtkcam_module_##_module_id_ {                          \
    auto_register_mtkcam_module_##_module_id_() {                             \
      mtkcam_module_info const info = {                                       \
          .module_id = _module_id_,                                           \
          .module_factory = reinterpret_cast<void*>(_factory_),               \
          .register_name = __BASE_FILE__,                                     \
      };                                                                      \
      register_mtkcam_module(                                                 \
          &info, NSCam::Int2Type<MTKCAM_GET_MODULE_GROUP_ID(_module_id_)>()); \
    }                                                                         \
  };                                                                          \
  static const auto_register_mtkcam_module_##_module_id_ singleton;           \
  }  // namespace

/*******************************************s***********************************
 *
 ******************************************************************************/
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_COMMON_MODULE_STORE_H_
