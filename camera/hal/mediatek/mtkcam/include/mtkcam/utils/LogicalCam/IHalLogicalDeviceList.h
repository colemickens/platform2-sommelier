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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_LOGICALCAM_IHALLOGICALDEVICELIST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_LOGICALCAM_IHALLOGICALDEVICELIST_H_

#include <mtkcam/def/common.h>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/LogicalCam/Type.h>
#include <mtkcam/utils/metadata/IMetadata.h>
//
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *  Hal Sensor List Interface.
 ******************************************************************************/
class VISIBILITY_PUBLIC IHalLogicalDeviceList {
 public:  ////                    Instantiation.
  static IHalLogicalDeviceList* get();

 protected:  ////                    Destructor.
  /**
   * Disallowed to directly delete a raw pointer.
   */
  virtual ~IHalLogicalDeviceList() {}

 public:  ////                    Attributes.
  /**
   * Query the number of logical devices.
   * This call is legal only after searchDevices().
   */
  virtual MUINT queryNumberOfDevices() const = 0;

  /**
   * Query the number of image sensors.
   * This call is legal only after searchDevices().
   */
  virtual MUINT queryNumberOfSensors() const = 0;

  /**
   * Query static information for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual IMetadata const& queryStaticInfo(MUINT const index) const = 0;

  /**
   * Query the driver name for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual char const* queryDriverName(MUINT const index) const = 0;

  /**
   * Query the sensor type of NSSensorType::Type for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual MUINT queryType(MUINT const index) const = 0;

  /**
   * Query the sensor facing direction for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual MUINT queryFacingDirection(MUINT const index) const = 0;

  /**
   * Query SensorDev Index by sensor list index.
   * This call is legal only after searchSensors().
   * Return SENSOR_DEV_MAIN, SENSOR_DEV_SUB,...
   */
  virtual MUINT querySensorDevIdx(MUINT const index) const = 0;

  /**
   * Query static SensorStaticInfo for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual SensorStaticInfo const* querySensorStaticInfo(
      MUINT const index) const = 0;

  /**
   * Query Sensor Information.
   * This call is legal only after searchSensors().
   */
  virtual MVOID querySensorStaticInfo(
      MUINT index, SensorStaticInfo* pSensorStaticInfo) const = 0;

  /**
   * Search sensors and return the number of logical devices.
   */
  virtual MUINT searchDevices() = 0;

  /**
   * get all sensor id belong this logical camera device
   * return index id list. (ex: 0,1,2)
   */
  virtual std::vector<MINT32> getSensorIds(MINT32 deviceId) = 0;

  /**
   * get logical device id for input sensor id
   */
  virtual MINT32 getDeviceId(MINT32 sensorId) = 0;

  /**
   * get sync type
   */
  virtual SensorSyncType getSyncType(MINT32 deviceId) const = 0;

  /**
   * get supported feature in specific instance id
   */
  virtual MINT32 getSupportedFeature(MINT32 deviceId) const = 0;

  /**
   * get master dev id
   */
  virtual MINT32 getSensorSyncMasterDevId(MINT32 deviceId) const = 0;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSCam

/**
 * @brief The definition of the maker of IHalLogicalDeviceList instance.
 */
typedef NSCam::IHalLogicalDeviceList* (*HalLogicalDeviceList_FACTORY_T)();

#define MAKE_HalLogicalDeviceList(...)                  \
  MAKE_MTKCAM_MODULE(MTKCAM_MODULE_ID_UTILS_LOGICALDEV, \
                     HalLogicalDeviceList_FACTORY_T, __VA_ARGS__)

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_UTILS_LOGICALCAM_IHALLOGICALDEVICELIST_H_
