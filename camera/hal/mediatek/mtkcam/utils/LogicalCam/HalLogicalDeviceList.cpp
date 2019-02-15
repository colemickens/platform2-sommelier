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

#define LOG_TAG "MtkCam/Util/LogicalDevice"

#include <camera_custom_logicaldevice.h>
#include <kd_imgsensor_define.h>
#include "MyUtils.h"
#include <mtkcam/utils/LogicalCam/IHalLogicalDeviceList.h>
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <map>
#include <memory>
#include <property_service/property.h>
#include <property_service/property_lib.h>
#include <string>
#include <sys/syscall.h>
#include <utility>
#include <vector>

using NSCam::IHalLogicalDeviceList;
using NSCam::IMetadata;
using NSCam::SensorStaticInfo;
using NSCam::SensorSyncType;

namespace NSCam {

/****************************************************************************
 *  Hal Sensor List Interface.
 ****************************************************************************/
class HalLogicalDeviceList : public IHalLogicalDeviceList {
 protected:  ////                    Destructor.
  /**
   * Disallowed to directly delete a raw pointer.
   */
  virtual ~HalLogicalDeviceList() {}

 public:  ////                    Attributes.
  /**
   * Query the number of logical devices.
   * This call is legal only after searchDevices().
   */
  MUINT queryNumberOfDevices() const override;

  /**
   * Query the number of image sensors.
   * This call is legal only after searchDevices().
   */
  MUINT queryNumberOfSensors() const override;

  /**
   * Query static information for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  IMetadata const& queryStaticInfo(MUINT const index) const override;

  /**
   * Query the driver name for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  char const* queryDriverName(MUINT const index) const override;

  /**
   * Query the sensor type of NSSensorType::Type for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  MUINT queryType(MUINT const index) const override;

  /**
   * Query the sensor facing direction for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  MUINT queryFacingDirection(MUINT const index) const override;

  /**
   * Query SensorDev Index by sensor list index.
   * This call is legal only after searchSensors().
   * Return SENSOR_DEV_MAIN, SENSOR_DEV_SUB,...
   */
  MUINT querySensorDevIdx(MUINT const index) const override;

  /**
   * Query static SensorStaticInfo for a specific sensor index.
   * This call is legal only after searchSensors().
   */
  SensorStaticInfo const* querySensorStaticInfo(
      MUINT const index) const override;

  /**
   * Query Sensor Information.
   * This call is legal only after searchSensors().
   */
  MVOID querySensorStaticInfo(
      MUINT index, SensorStaticInfo* pSensorStaticInfo) const override;

  /**
   * Search sensors and return the number of image sensors.
   */
  MUINT searchDevices() override;

  std::vector<MINT32> getSensorIds(MINT32 deviceId) override;

  MINT32 getDeviceId(MINT32 sensorId) override;

  MINT32 getSupportedFeature(MINT32 deviceId) const override;

  SensorSyncType getSyncType(MINT32 deviceId) const override;

  MINT32 getSensorSyncMasterDevId(MINT32 deviceId) const override;

 private:
  struct TempSensorInfo {
    MINT32 SensorId;
    MINT32 RawType;
    MINT32 Facing;
    MINT32 CaptureModeWidth;
    char Name[128];
  };
  using SensorInfo_t = std::map<std::string, struct TempSensorInfo>;

  class SyncTypeInfo {
   public:
    SensorSyncType syncType = SensorSyncType::NOT_SUPPORT;
    MUINT32 masterDevId = 0xFF;
    std::vector<MUINT32> syncMode;
    std::vector<MUINT32>
        slaveDevId;  // only one master, but can have more than 1 slave.
  };

  //
  // local function
  MINT32 createDeviceMap();
  MINT32 addLogicalDevice(SensorInfo_t vInfo,
                          struct LogicalSensorStruct* pLogicalSen,
                          MINT32 DevNum);
  void dumpDebugInfo();
  // query sync mode
  std::shared_ptr<SyncTypeInfo> querySyncMode(
      const std::vector<MINT32>& Sensors);

  class CamDeviceInfo {
   public:
    std::vector<MINT32> Sensors;
    MUINT SupportedFeature;
    MINT32 RawType;
    char Name[192];
    std::shared_ptr<SyncTypeInfo> syncTypeInfo = nullptr;
  };

  std::map<MINT32, std::shared_ptr<CamDeviceInfo> > mDeviceSensorMap;
  mutable std::mutex mLock;
};

};  // namespace NSCam

using NSCam::HalLogicalDeviceList;

/******************************************************************************
 *
 ******************************************************************************/
IHalLogicalDeviceList* IHalLogicalDeviceList::get() {
  static HalLogicalDeviceList* gInst = new HalLogicalDeviceList();
  return gInst;
}

MINT32
HalLogicalDeviceList::addLogicalDevice(SensorInfo_t vInfo,
                                       struct LogicalSensorStruct* pLogicalSen,
                                       MINT32 DevNum) {
  std::shared_ptr<CamDeviceInfo> Info = std::make_shared<CamDeviceInfo>();
  const char* Main1Name;
  // check sensor count is match to NumofCombinSensor.
  if ((pLogicalSen->Sensorlist.size()) % (pLogicalSen->NumofCombinSensor) !=
      0) {
    MY_LOGE("Sensor list count(%d) does not match to combin sensor count(%d)",
            pLogicalSen->Sensorlist.size(), pLogicalSen->NumofCombinSensor);
    return -1;
  }
  for (int i = 0; i < pLogicalSen->NumofCombinSensor; i++) {
    auto it = vInfo.find(
        pLogicalSen->Sensorlist[DevNum * pLogicalSen->NumofCombinSensor + i]);
    if (it == vInfo.end()) {
      return -1;
    }
    Info->Sensors.push_back((it->second).SensorId);
    if (i == 0) {
      Main1Name = (it->second).Name;
    }
  }
  Info->SupportedFeature = pLogicalSen->Feature;
  Info->RawType = SENSOR_RAW_Bayer;
  Info->syncTypeInfo = querySyncMode(Info->Sensors);
  snprintf(Info->Name, sizeof(Info->Name), "%s_%s", Main1Name,
           pLogicalSen->Name);
  MY_LOGI("add new logic device: %s", Info->Name);
  mDeviceSensorMap.emplace(std::make_pair(mDeviceSensorMap.size(), Info));
  return 0;
}

void HalLogicalDeviceList::dumpDebugInfo() {
  MY_LOGI_IF(1, "map size : %d", mDeviceSensorMap.size());
  for (int i = 0; i < mDeviceSensorMap.size(); i++) {
    MY_LOGI_IF(1, "index(%d) name : %s", i, queryDriverName(i));
    MY_LOGI_IF(1, "index(%d) facing : %d", i, queryFacingDirection(i));
  }
}

std::shared_ptr<HalLogicalDeviceList::SyncTypeInfo>
HalLogicalDeviceList::querySyncMode(const std::vector<MINT32>& Sensors) {
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  std::string checkString("");
  // SensorSyncType result;
  std::shared_ptr<SyncTypeInfo> info = std::make_shared<SyncTypeInfo>();
  std::vector<bool> masterCheckList;
  std::vector<bool> slaveCheckList;
  std::vector<MUINT32> devIdList;
  for (auto& sensorId : Sensors) {
    NSCam::IHalSensor* pHalSensor =
        pHalSensorList->createSensor(LOG_TAG, sensorId);
    MUINT32 syncMode;
    MUINT32 sensorDevId = pHalSensorList->querySensorDevIdx(sensorId);
    pHalSensor->sendCommand(sensorDevId,
                            SENSOR_CMD_GET_SENSOR_SYNC_MODE_CAPACITY,
                            (MUINTPTR)&syncMode, sizeof(syncMode), 0, 0, 0, 0);
    info->syncMode.push_back(syncMode);
    if ((syncMode & SENSOR_MASTER_SYNC_MODE) &&
        (syncMode & SENSOR_SLAVE_SYNC_MODE)) {
      masterCheckList.push_back(true);
      slaveCheckList.push_back(true);
    } else if (syncMode & SENSOR_MASTER_SYNC_MODE) {
      masterCheckList.push_back(true);
      slaveCheckList.push_back(false);
    } else if (syncMode & SENSOR_SLAVE_SYNC_MODE) {
      masterCheckList.push_back(false);
      slaveCheckList.push_back(true);
    } else {
      masterCheckList.push_back(false);
      slaveCheckList.push_back(false);
    }
    devIdList.push_back(sensorDevId);
    base::StringAppendF(&checkString, "S[%d:D%d:M%d:S%d] ", sensorId,
                        sensorDevId, !!(syncMode & SENSOR_MASTER_SYNC_MODE),
                        !!(syncMode & SENSOR_SLAVE_SYNC_MODE));
  }
  ssize_t masterIndex = -1;
  ssize_t slaveIndex = -1;
  for (ssize_t i = 0; i < masterCheckList.size(); ++i) {
    if (masterCheckList[i]) {
      masterIndex = i;
      for (ssize_t j = 0; j < slaveCheckList.size(); ++j) {
        if (i != j) {
          if (slaveCheckList[j]) {
            slaveIndex = j;
            break;
          }
        }
      }
      if (slaveIndex != -1) {
        break;
      }
    }
  }
  // set master id
  if (masterIndex != -1) {
    info->masterDevId = devIdList[masterIndex];
    base::StringAppendF(&checkString, "M[D:%d] ", devIdList[masterIndex]);
  }
  for (ssize_t i = 0; i < slaveCheckList.size(); ++i) {
    if ((i != masterIndex) && slaveCheckList[i]) {
      info->slaveDevId.push_back(devIdList[i]);
      base::StringAppendF(&checkString, "S[D:%d] ", devIdList[i]);
    }
  }
  base::StringAppendF(&checkString, "Master[D:%d] SlaveList[%d]",
                      info->masterDevId, info->slaveDevId.size());
  if (info->masterDevId != 0xFF &&
      info->slaveDevId.size() == (Sensors.size() - 1)) {
    info->syncType = SensorSyncType::CALIBRATED;
    base::StringAppendF(&checkString, "R[Calibrated]");
  } else {
    info->syncType = SensorSyncType::APPROXIMATE;
    base::StringAppendF(&checkString, "R[Approximate]");
  }
  MY_LOGI("%s", checkString.c_str());
  return info;
}

MINT32
HalLogicalDeviceList::createDeviceMap() {
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  size_t const sensorNum = pHalSensorList->searchSensors();
  SensorInfo_t vTempInfo;
  TempSensorInfo TempInfo;
  SensorStaticInfo sensorStaticInfo;
  MINT32 i = 0;

  MY_LOGD("sensorNum : %d", sensorNum);
  for (i = 0; i < sensorNum; i++) {
    memset(&sensorStaticInfo, 0, sizeof(SensorStaticInfo));
    int sendorDevIndex = pHalSensorList->querySensorDevIdx(i);
    pHalSensorList->querySensorStaticInfo(sendorDevIndex, &sensorStaticInfo);

    TempInfo.SensorId = i;
    TempInfo.RawType = sensorStaticInfo.rawFmtType;
    TempInfo.Facing = sensorStaticInfo.facingDirection;
    TempInfo.CaptureModeWidth = sensorStaticInfo.captureWidth;
    snprintf(TempInfo.Name, sizeof(TempInfo.Name), "%s",
             pHalSensorList->queryDriverName(i));

    vTempInfo.emplace(std::make_pair(TempInfo.Name, TempInfo));
    MY_LOGD("i : %d, facing : %d", i, sensorStaticInfo.facingDirection);
    MY_LOGD("i : %d, Name : %s", i, TempInfo.Name);
    MY_LOGD("i : %d, vTempInfo Name : %s", i, vTempInfo[TempInfo.Name].Name);

    std::shared_ptr<CamDeviceInfo> Info = std::make_shared<CamDeviceInfo>();
    Info->Sensors.push_back(i);
    Info->SupportedFeature = 0;
    Info->RawType = TempInfo.RawType;
    snprintf(Info->Name, sizeof(Info->Name), "%s", TempInfo.Name);

    MY_LOGD("i : %d, Info Name : %s, %p", i, Info->Name, Info->Name);

    mDeviceSensorMap.emplace(std::make_pair(i, Info));
  }

  dumpDebugInfo();
  return 0;
}

MUINT
HalLogicalDeviceList::queryNumberOfDevices() const {
  std::lock_guard<std::mutex> _l(mLock);
  return mDeviceSensorMap.size();
}

MUINT
HalLogicalDeviceList::queryNumberOfSensors() const {
  std::lock_guard<std::mutex> _l(mLock);
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  return pHalSensorList->queryNumberOfSensors();
}

IMetadata const& HalLogicalDeviceList::queryStaticInfo(
    MUINT const index) const {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[queryStaticInfo] mDeviceSensorMap.size() == 0");
  }
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  std::shared_ptr<CamDeviceInfo> info = mDeviceSensorMap.at(index);

  return pHalSensorList->queryStaticInfo(info->Sensors[0]);
}

char const* HalLogicalDeviceList::queryDriverName(MUINT const index) const {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[queryDriverName] mDeviceSensorMap.size() == 0");
  }
  // IHalSensorList* const pHalSensorList = GET_HalSensorList();
  MY_LOGD("queryDriverName index : %d", index);
  std::shared_ptr<CamDeviceInfo> info = mDeviceSensorMap.at(index);
  MY_LOGD("queryDriverName : %s, %p", info->Name, info->Name);

  return info->Name;
}

MUINT
HalLogicalDeviceList::queryType(MUINT const index) const {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[queryType] mDeviceSensorMap.size() == 0");
  }
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  std::shared_ptr<CamDeviceInfo> info = mDeviceSensorMap.at(index);
  return pHalSensorList->queryType(info->Sensors[0]);
}

MUINT
HalLogicalDeviceList::queryFacingDirection(MUINT const index) const {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[queryFacingDirection] mDeviceSensorMap.size() == 0");
  }
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  std::shared_ptr<CamDeviceInfo> info = mDeviceSensorMap.at(index);

  return pHalSensorList->queryFacingDirection(info->Sensors[0]);
}

MUINT
HalLogicalDeviceList::querySensorDevIdx(MUINT const index) const {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[querySensorDevIdx] mDeviceSensorMap.size() == 0");
  }
  IHalSensorList* const pHalSensorList = GET_HalSensorList();
  std::shared_ptr<CamDeviceInfo> info = mDeviceSensorMap.at(index);

  return pHalSensorList->querySensorDevIdx(info->Sensors[0]);
}

SensorStaticInfo const* HalLogicalDeviceList::querySensorStaticInfo(
    MUINT const index) const {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[querySensorStaticInfo] mDeviceSensorMap.size() == 0");
  }
  MY_LOGD("queryDriverName1 index : %d", index);
  IHalSensorList* const pHalSensorList = GET_HalSensorList();

  return pHalSensorList->querySensorStaticInfo(index);
}

MVOID
HalLogicalDeviceList::querySensorStaticInfo(
    MUINT index, SensorStaticInfo* pSensorStaticInfo) const {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[querySensorStaticInfo] mDeviceSensorMap.size() == 0");
  }
  MY_LOGD("queryDriverName2 index : %d", index);
  IHalSensorList* const pHalSensorList = GET_HalSensorList();

  pHalSensorList->querySensorStaticInfo(index, pSensorStaticInfo);
}

MUINT
HalLogicalDeviceList::searchDevices() {
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGD("Create logical device map");
    createDeviceMap();
  }

  return mDeviceSensorMap.size();
}

std::vector<MINT32> HalLogicalDeviceList::getSensorIds(MINT32 deviceId) {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[getSensorIds] mDeviceSensorMap.size() == 0");
  }
  std::shared_ptr<CamDeviceInfo> info = mDeviceSensorMap.at(deviceId);
  return info->Sensors;
}

MINT32
HalLogicalDeviceList::getDeviceId(MINT32 sensorId) {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[getDeviceId] mDeviceSensorMap.size() == 0");
  }

  auto it = mDeviceSensorMap.begin();
  for (int i = 0; i < mDeviceSensorMap.size(); i++, it++) {
    std::shared_ptr<CamDeviceInfo> info = it->second;
    if (info->Sensors.size() == 1 && info->Sensors[0] == sensorId) {
      return i;
    }
  }

  return -1;
}

MINT32
HalLogicalDeviceList::getSupportedFeature(MINT32 deviceId) const {
  std::lock_guard<std::mutex> _l(mLock);
  if (mDeviceSensorMap.size() == 0) {
    MY_LOGE("[getDeviceId] mDeviceSensorMap.size() == 0");
  }
  std::shared_ptr<CamDeviceInfo> info = mDeviceSensorMap.at(deviceId);
  return info->SupportedFeature;
}

SensorSyncType HalLogicalDeviceList::getSyncType(MINT32 deviceId) const {
  std::lock_guard<std::mutex> _l(mLock);
  std::shared_ptr<CamDeviceInfo> info = mDeviceSensorMap.at(deviceId);
  return info->syncTypeInfo->syncType;
}

MINT32
HalLogicalDeviceList::getSensorSyncMasterDevId(MINT32 deviceId) const {
  std::lock_guard<std::mutex> _l(mLock);
  std::shared_ptr<CamDeviceInfo> info = mDeviceSensorMap.at(deviceId);
  return info->syncTypeInfo->masterDevId;
}
