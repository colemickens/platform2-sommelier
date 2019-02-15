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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_SYS_SENSORPROVIDER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_SYS_SENSORPROVIDER_H_

#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <mtkcam/utils/sys/sensor_type.h>

namespace NSCam {
namespace Utils {
class SensorProviderService;

class SensorProvider {
 public:
  /** Create SensorProvider instance.
   */
  static std::shared_ptr<SensorProvider> createInstance(const char* user);

  /** Check whether sensor enabled successfully
   *   @sensorType: sensorType (Gyro/Acceleration)
   *   @This function is Thread-safe
   */
  MBOOL isEnabled(const eSensorType sensorType);

  /** Register and enable sensor
   *   @sensorType: sensorType (Gyro/Acceleration)
   *   @intervalInMs: frequency of sensor event callbacks
   *   @This function is Thread-safe
   */
  MBOOL enableSensor(const eSensorType sensorType, const MUINT32 intervalInMs);

  /** Unregister and disable sensor
   *   @sensorType: sonsorType (Gyro/Acceleration)
   *   @This function is Thread-safe
   */
  MBOOL disableSensor(const eSensorType sensorType);

  /** Get latest sensor data
   *   @sensorType: sensorType (Gyro/Acceleration)
   *   @sensorData: structure used for storing sensor data
   *   @This function is Thread-safe
   */
  MBOOL getLatestSensorData(const eSensorType sensorType,
                            SensorData* sensorData);

  /** Get all sensor data in queue and clear queue
   *   @sensorType: sensorType (Gyro/Acceleration)
   *   @sensorData: vector of structure used for storing sensor data
   *   @This function is Thread-safe
   */
  MBOOL getAllSensorData(const eSensorType sensorType,
                         std::vector<SensorData>* sensorData);

 private:
  struct SensorState {
    SensorState() : interval(0), serialNum(-1) {}
    MUINT32 interval;
    MINT64 serialNum;
  };

  explicit SensorProvider(const char* user);

  virtual ~SensorProvider();

  std::shared_ptr<SensorProviderService> mpSensorProviderService;
  std::string mUser;
  std::map<eSensorType, SensorState> mSensorState;
  std::mutex mLock;
};
}  // namespace Utils
}  // namespace NSCam

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_SYS_SENSORPROVIDER_H_
