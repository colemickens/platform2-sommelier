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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_SYS_SENSOR_TYPE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_SYS_SENSOR_TYPE_H_

namespace NSCam {
namespace Utils {

struct SensorData {
  SensorData() : acceleration{0}, gyro{0}, timestamp(0) {}
  MFLOAT acceleration[3];
  MFLOAT gyro[3];
  MINT64 timestamp;
};

enum eSensorType {
  SENSOR_TYPE_GYRO = 0,
  SENSOR_TYPE_ACCELERATION,
  SENSOR_TYPE_COUNT,
};

enum eSensorStatus {
  STATUS_UNINITIALIZED = 0,
  STATUS_INITIALIZED,
  STATUS_ERROR,
};

}  // namespace Utils
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_SYS_SENSOR_TYPE_H_
