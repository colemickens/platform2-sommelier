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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_MYUTILS_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_MYUTILS_H_

/******************************************************************************
 *
 ******************************************************************************/
//
#include <atomic>
#include <fcntl.h>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#if '1' == MTKCAM_USE_LEGACY_SENSOR_HAL
#include <sensor_hal.h>
#endif
//
#include "custom/Info.h"
#include "HalSensorList.h"
#include "HalSensor.h"
using NSCam::IHalSensor;
using NSCam::IHalSensorList;
using NSCam::NSHalSensor::HalSensor;
using NSCam::NSHalSensor::HalSensorList;
using NSCam::NSHalSensor::Info;
using std::string;
//
//
#include <mtkcam/utils/std/Log.h>
//
//
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_MYUTILS_H_
