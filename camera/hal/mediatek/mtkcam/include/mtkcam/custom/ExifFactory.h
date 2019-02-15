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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_CUSTOM_EXIFFACTORY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_CUSTOM_EXIFFACTORY_H_
//
#include <custom/debug_exif/IDebugExif.h>
#include <mtkcam/utils/module/module.h>
//
/**
 * @brief The definition of the maker of IDebugExif instance.
 */
static inline auto MAKE_DebugExif() {
  typedef NSCam::Custom::IDebugExif* (*FACTORY_T)();
  return MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_CUSTOM_DEBUG_EXIF, FACTORY_T);
}

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_CUSTOM_EXIFFACTORY_H_
