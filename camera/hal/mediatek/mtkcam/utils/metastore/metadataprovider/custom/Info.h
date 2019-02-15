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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METASTORE_METADATAPROVIDER_CUSTOM_INFO_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METASTORE_METADATAPROVIDER_CUSTOM_INFO_H_
//
#include <stdint.h>

#ifndef VISIBILITY_PUBLIC
#define VISIBILITY_PUBLIC __attribute__((visibility("default")))
#endif

/******************************************************************************
 *
 ******************************************************************************/
namespace NSMetadataProvider {

/******************************************************************************
 *
 ******************************************************************************/
void showCustInfo();

/******************************************************************************
 *  Info
 ******************************************************************************/
class Info {
 protected:  ////    Data Members.
  int32_t mSensorIndex;
  int32_t mSensorDev;
  int32_t mSensorType;
  char const* mSensorDrvName;

 public:  ////    Instantiation.
  Info()
      : mSensorIndex(0),
        mSensorDev(0),
        mSensorType(0),
        mSensorDrvName(nullptr) {}
  //
  Info(unsigned int const uSensorIndex,
       unsigned int const uSensorDev,
       unsigned int const eSensorType,
       char const* szSensorDrvName)
      : mSensorIndex(uSensorIndex),
        mSensorDev(uSensorDev),
        mSensorType(eSensorType),
        mSensorDrvName(szSensorDrvName) {}

 public:  ////    Accessors.
  int32_t getDeviceId() const { return mSensorIndex; }
  int32_t getSensorDev() const { return mSensorDev; }
  int32_t getSensorType() const { return mSensorType; }
  char const* getSensorDrvName() const { return mSensorDrvName; }
};

/******************************************************************************
 *  Static Metadata
 ******************************************************************************/
#define STATIC_METADATA_BEGIN(PREFIX, TYPE, SENSORNAME)             \
  VISIBILITY_PUBLIC extern "C" status_t                             \
      constructCustStaticMetadata_##PREFIX##_##TYPE##_##SENSORNAME( \
          NSCam::IMetadata* rMetadata, Info const& rInfo) {
#define STATIC_METADATA_END() \
  return OK;                  \
  }

#define STATIC_METADATA2_BEGIN(PREFIX, TYPE, SENSORNAME)            \
  VISIBILITY_PUBLIC extern "C" status_t                             \
      constructCustStaticMetadata_##PREFIX##_##TYPE##_##SENSORNAME( \
          NSCam::IMetadata* rMetadata) {
#define STATIC_METADATA_END() \
  return OK;                  \
  }

#define PREFIX_FUNCTION_STATIC_METADATA "constructCustStaticMetadata"

/******************************************************************************
 *
 ******************************************************************************/
static char const* const kStaticMetadataTypeNames[] = {
    // "LENS",
    // "SENSOR",
    // "TUNING_3A",
    // "FLASHLIGHT",
    "SCALER", "FEATURE", "CAMERA", "REQUEST", nullptr};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSMetadataProvider
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_UTILS_METASTORE_METADATAPROVIDER_CUSTOM_INFO_H_
