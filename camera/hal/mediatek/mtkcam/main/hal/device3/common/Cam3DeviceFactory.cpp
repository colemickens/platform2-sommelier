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

#define LOG_TAG "MtkCam/Cam3DeviceFactory"
//
#include <memory>
#include <string>

#include "MyUtils.h"
#include <mtkcam/main/hal/Cam3Device.h>
//

extern std::shared_ptr<NSCam::Cam3Device> createCam3Device_Default(
    std::string const& rDevName, int32_t const i4OpenId);
/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<NSCam::Cam3Device> createCam3Device(
    std::string const s8ClientAppMode, int32_t const i4OpenId) {
  std::shared_ptr<NSCam::Cam3Device> pdev =
      createCam3Device_Default(s8ClientAppMode, i4OpenId);
  return pdev;
}
