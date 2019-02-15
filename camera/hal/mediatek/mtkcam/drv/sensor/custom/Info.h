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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_CUSTOM_INFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_CUSTOM_INFO_H_
//
#include <string>

typedef unsigned int uint_t;

#ifndef VISIBILITY_PUBLIC
#define VISIBILITY_PUBLIC __attribute__((visibility("default")))
#endif

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace NSHalSensor {

/******************************************************************************
 *
 ******************************************************************************/
void showCustInfo();

/******************************************************************************
 *  Info
 ******************************************************************************/
class Info {
 public:  ////                    Accessors.
  void setDeviceId(uint_t idx) { muSensorIndex = idx; }
  void setSensorType(uint_t type) { meSensorType = type; }
  void setSensorDrvName(std::string name) { ms8SensorDrvName = name; }

  uint_t getDeviceId() const { return muSensorIndex; }
  uint_t getSensorType() const { return meSensorType; }
  std::string const& getSensorDrvName() const { return ms8SensorDrvName; }

 public:  ////                    Instantiation.
  Info()
      : muSensorIndex(0), meSensorType(0), ms8SensorDrvName(std::string("0")) {}
  Info(uint_t const uSensorIndex,
       uint_t const eSensorType,
       char const* szSensorDrvName)
      : muSensorIndex(uSensorIndex),
        meSensorType(eSensorType),
        ms8SensorDrvName(std::string(szSensorDrvName)) {}

 protected:  ////                    Data Members.
  uint_t muSensorIndex;
  uint_t meSensorType;           // NSSensorType::Type
  std::string ms8SensorDrvName;  // SENSOR_DRVNAME_xxx
};

/******************************************************************************
 *  Static Metadata
 ******************************************************************************/
#define STATIC_METADATA_BEGIN(PREFIX, TYPE, SENSORNAME)             \
  VISIBILITY_PUBLIC extern "C" MBOOL                                \
      constructCustStaticMetadata_##PREFIX##_##TYPE##_##SENSORNAME( \
          NSCam::IMetadata* pMetadata, Info const& rInfo) {         \
    IMetadata& rMetadata = *pMetadata;
#define STATIC_METADATA_END() \
  return MTRUE;               \
  }

#define STATIC_METADATA2_BEGIN(PREFIX, TYPE, SENSORNAME)            \
  VISIBILITY_PUBLIC extern "C" MBOOL                                \
      constructCustStaticMetadata_##PREFIX##_##TYPE##_##SENSORNAME( \
          NSCam::IMetadata* pMetadata) {                            \
    IMetadata& rMetadata = *pMetadata;
#define STATIC_METADATA_END() \
  return MTRUE;               \
  }

#define PREFIX_FUNCTION_STATIC_METADATA "constructCustStaticMetadata"

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSHalSensor
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_SENSOR_CUSTOM_INFO_H_
