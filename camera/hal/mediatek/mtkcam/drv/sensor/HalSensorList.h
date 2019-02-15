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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_HALSENSORLIST_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_HALSENSORLIST_H_

#include "custom/Info.h"
#include <mtkcam/drv/IHalSensor.h>
#include "HalSensor.h"
#include <list>
#include <string>
#include <vector>
#include <linux/media.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSHalSensor {
/******************************************************************************
 *  Hal Sensor List.
 ******************************************************************************/
class VISIBILITY_PUBLIC HalSensorList : public IHalSensorList {
 public:  ////                    Attributes.
  /**
   * Query the number of image sensors.
   * This call is legal only after searchSensors().
   */
  virtual MUINT queryNumberOfSensors() const;

  /**
   * Query static information for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual IMetadata const& queryStaticInfo(MUINT const index) const;

  /**
   * Query the driver name for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual char const* queryDriverName(MUINT const index) const;

  /**
   * Query the sensor type of NSSensorType::Type for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual MUINT queryType(MUINT const index) const;

  /**
   * Query the sensor facing direction for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual MUINT queryFacingDirection(MUINT const index) const;

  /**
   * Query SensorDev Index by sensor list index.
   * This call is legal only after searchSensors().
   * Return SENSOR_DEV_MAIN, SENSOR_DEV_SUB,...
   */
  virtual MUINT querySensorDevIdx(MUINT const index) const;

  /**
   * Query static SensorStaticInfo for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual SensorStaticInfo const* querySensorStaticInfo(
      MUINT const indexDual) const;

  /**
   * Search sensors and return the number of image sensors.
   */
  virtual MUINT searchSensors();

  /**
   * Create an instance of IHalSensor for a single specific sensor index.
   * This call is legal only after searchSensors().
   */
  virtual IHalSensor* createSensor(char const* szCallerName, MUINT const index);

  /**
   * Create an instance of IHalSensor for multiple specific sensor indexes.
   * This call is legal only after searchSensors().
   */
  virtual IHalSensor* createSensor(char const* szCallerName,
                                   MUINT const uCountOfIndex,
                                   MUINT const* pArrayOfIndex);

  /**
   * Query sneosr related information for a specific sensor indexes.
   * This call is legal only after searchSensors().
   */

  virtual MVOID querySensorStaticInfo(
      MUINT indexDual, SensorStaticInfo* pSensorStaticInfo) const;

  virtual const char* queryDevName();
  virtual int queryP1NodeEntId();
  virtual int querySeninfEntId();
  virtual int querySensorEntId(MUINT const index);
  virtual const char* querySeninfSubdevName();
  virtual const char* querySensorSubdevName(MUINT const index);
  virtual int querySeninfFd();
  virtual int querySensorFd(MUINT const index);
  virtual MVOID setSeninfFd(int fd);
  virtual MVOID setSensorFd(int fd, MUINT const index);
  virtual int findSubdev(void);
  virtual struct imgsensor_info_struct* getSensorInfo(IMGSENSOR_SENSOR_IDX idx);
  virtual MUINT32 getSensorType(IMGSENSOR_SENSOR_IDX idx);
  virtual SENSOR_WINSIZE_INFO_STRUCT* getWinSizeInfo(IMGSENSOR_SENSOR_IDX idx,
                                                     MUINT32 scenario) const;
  virtual const char* getSensorName(IMGSENSOR_SENSOR_IDX idx);
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Enum Sensor List.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Definitions.
  class EnumInfo : public Info {
   public:
    IMetadata mMetadata;
  };

  EnumInfo const* queryEnumInfoByIndex(MUINT index) const;
  static HalSensorList* singleton();
  virtual MVOID closeSensor(HalSensor* pHalSensor, char const* szCallerName);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Open Sensor List.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  struct OpenInfo {
    volatile MINT miRefCount;
    HalSensor* mpHalSensor;

    OpenInfo();
    OpenInfo(MINT iRefCount, HalSensor* pHalSensor);
  };
  typedef std::list<OpenInfo> OpenList_t;

 protected:  ////                    Instantiation.
  virtual ~HalSensorList() {}
  HalSensorList();

  virtual HalSensor* openSensor(std::vector<MUINT> const& vSensorIndex,
                                char const* szCallerName);

  /**
   * Build static information for a specific sensor.
   */
  EnumInfo const* addAndInitSensorEnumInfo_Locked(
      IMGSENSOR_SENSOR_IDX eSensorDev,
      MUINT eSensorType,
      const char* szSensorDrvName);
  virtual MBOOL buildStaticInfo(Info const& rInfo, IMetadata* pMetadata) const;
  virtual MVOID querySensorInfo(IMGSENSOR_SENSOR_IDX sensorDev);
  virtual MVOID buildSensorMetadata(IMGSENSOR_SENSOR_IDX sensorDev);
  const SensorStaticInfo* gQuerySensorStaticInfo(
      IMGSENSOR_SENSOR_IDX sensorIDX) const;

 protected:  ////                    Data Members.
  mutable std::mutex mOpenSensorMutex;
  OpenList_t mOpenSensorList;
  mutable std::mutex mEnumSensorMutex;
  vector<EnumInfo> mEnumSensorList;
  SensorStaticInfo sensorStaticInfo[IMGSENSOR_SENSOR_IDX_MAX_NUM];
  std::string sensorSubdevName[IMGSENSOR_SENSOR_IDX_MAX_NUM];
  std::string seninfSubdevName;
  std::string p1NodeName;
  std::string devName;
  int sensorEntId[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {0};
  int seninfEntId = 0;
  int p1NodeEntId = 0;
  int sensorFd[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {0};
  int seninfFd = 0;
  int p1NodeFd = 0;
  int sensor_nums = 0;
  MUINT32 sensorId[IMGSENSOR_SENSOR_IDX_MAX_NUM] = {0};
};
};      // namespace NSHalSensor
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_HALSENSORLIST_H_
