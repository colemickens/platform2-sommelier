/*
 * Copyright (C) 2019 Mediatek Corporation.
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

#define LOG_TAG "IPCHalSensorList"

#include <mtkcam/v4l2/IPCIHalSensor.h>

// MTKCAM
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>
#include <mtkcam/utils/metadata/IMetadata.h>
#include <mtkcam/utils/std/compiler.h>  // CC_LIKELY, CC_UNLIKELY
#include <mtkcam/utils/std/Log.h>       // CAM_LOGD, CAM_LOGI...etc

// STL
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

using NSCam::IHalSensor;
using NSCam::IIPCHalSensor;
using NSCam::IIPCHalSensorListProv;
using NSCam::IMetadata;
using NSCam::SensorStaticInfo;

// Implementation of IIPCHalSensorList
class IPCHalSensorListImpProv : public IIPCHalSensorListProv {
 public:
  IPCHalSensorListImpProv();
  virtual ~IPCHalSensorListImpProv();

  // Implementation of IIPCHalSensorList
 public:
  void ipcSetSensorStaticInfo(MUINT32 idx,
                              MUINT32 type,
                              MUINT32 deviceId,
                              const SensorStaticInfo& info) override;
  void ipcSetStaticInfo(MUINT32 idx, const IMetadata& info) override;

  // Implementation of IHalSensorList
 public:
  MUINT queryNumberOfSensors() const override;

  IMetadata const& queryStaticInfo(MUINT const index) const override;

  char const* queryDriverName(MUINT const index) const override;

  MUINT queryType(MUINT const index) const override;

  MUINT queryFacingDirection(MUINT const index) const override;

  MUINT querySensorDevIdx(MUINT const index) const override;

  SensorStaticInfo const* querySensorStaticInfo(
      MUINT const indexDual) const override;

  MVOID querySensorStaticInfo(
      MUINT indexDual, SensorStaticInfo* pSensorStaticInfo) const override;

  MUINT searchSensors() override;

  IHalSensor* createSensor(char const* szCallerName,
                           MUINT const index) override;

  IHalSensor* createSensor(char const* szCallerName,
                           MUINT const uCountOfIndex,
                           MUINT const* pArrayOfIndex) override;

  // attributes
 private:
  struct MySensorInfo {
    uint32_t type;
    uint32_t deviceId;
    SensorStaticInfo info;
    IMetadata staticInfo;
    std::shared_ptr<IHalSensor> halSensor;
    //
    MySensorInfo() : type(0), deviceId(0) {}
    ~MySensorInfo() = default;
  };

  mutable std::mutex m_sensorsLock;
  mutable std::unordered_map<uint32_t, MySensorInfo> m_sensorsInfo;
};

/******************************************************************************
   I M P L E M E N T A T I O N    O F    I I P C H A L S E N S O R L I S T
 ******************************************************************************/
IIPCHalSensorListProv* IIPCHalSensorListProv::getInstance() {
  static IPCHalSensorListImpProv* impl = new IPCHalSensorListImpProv();
  return impl;
}

IIPCHalSensor* IIPCHalSensorListProv::createIIPCSensor(int index) {
  return IIPCHalSensor::createInstance(index);
}

/******************************************************************************
   I M P L E M E N T A T I O N    O F    I P C H A L S E N S O R L I S T I M P
 ******************************************************************************/
IPCHalSensorListImpProv::IPCHalSensorListImpProv() {
  m_sensorsInfo.reserve(2);  // we may have at least 2 sensors
}

IPCHalSensorListImpProv::~IPCHalSensorListImpProv() {}

void IPCHalSensorListImpProv::ipcSetStaticInfo(MUINT32 idx,
                                               const IMetadata& info) {
  std::lock_guard<std::mutex> lk(m_sensorsLock);

  m_sensorsInfo[idx].staticInfo = info;

  std::atomic_thread_fence(std::memory_order_release);
}

void IPCHalSensorListImpProv::ipcSetSensorStaticInfo(
    MUINT32 idx, MUINT32 type, MUINT32 deviceId, const SensorStaticInfo& info) {
  std::lock_guard<std::mutex> lk(m_sensorsLock);

  m_sensorsInfo[idx].type = type;
  m_sensorsInfo[idx].deviceId = deviceId;
  m_sensorsInfo[idx].info = info;

  std::atomic_thread_fence(std::memory_order_release);
}

/******************************************************************************
   I M P L E M E N T A T I O N    O F    I P C H A L S E N S O R L I S T
 ******************************************************************************/
MUINT
IPCHalSensorListImpProv::searchSensors() {
  // no need to impl so far
  MY_LOGE("%s not impl", __FUNCTION__);
  return 0;
}

MUINT
IPCHalSensorListImpProv::queryNumberOfSensors() const {
  // no referenced by aaa (excluding CCT or ACDK tools)
  MY_LOGE("%s not impl", __FUNCTION__);
  return 0;
}

IMetadata const& IPCHalSensorListImpProv::queryStaticInfo(
    MUINT const index) const {
  static IMetadata m;
  MY_LOGE("%s not impl", __FUNCTION__);
  return m;
}

char const* IPCHalSensorListImpProv::queryDriverName(MUINT const index) const {
  return "IPCHalSensorList";
  // only ACDK uses this method. No need.
}

MUINT
IPCHalSensorListImpProv::queryType(MUINT const index) const {
  // need, since Hal3AFlowCtrl referenced this
  std::lock_guard<std::mutex> lk(m_sensorsLock);
  std::atomic_thread_fence(std::memory_order_acquire);

  auto itr = m_sensorsInfo.find(index);
  if (CC_UNLIKELY(itr == m_sensorsInfo.end())) {
    MY_LOGE("sensor type not found (idx=%u)", index);
    return 0;
  }

  return itr->second.type;
}

MUINT
IPCHalSensorListImpProv::queryFacingDirection(MUINT const index) const {
  MY_LOGE("%s not impl", __FUNCTION__);
  return 0;
}

MUINT
IPCHalSensorListImpProv::querySensorDevIdx(MUINT const index) const {
  std::lock_guard<std::mutex> lk(m_sensorsLock);
  std::atomic_thread_fence(std::memory_order_acquire);

  auto itr = m_sensorsInfo.find(index);
  if (CC_UNLIKELY(itr == m_sensorsInfo.end())) {
    MY_LOGE("device ID not found. (idx=%u)", index);
    return 0;
  }
  return IMGSENSOR_SENSOR_IDX2DUAL(itr->second.deviceId);
}

MVOID
IPCHalSensorListImpProv::querySensorStaticInfo(
    MUINT devId, SensorStaticInfo* pSensorStaticInfo)
    const {  // Note: devId is a sensor device ID, is necessary to be converted
             // to sensor index

  if (CC_UNLIKELY(pSensorStaticInfo == nullptr)) {
    MY_LOGE("%s failed since no pSensorStaticInfo", __FUNCTION__);
    return;
  }

  IMGSENSOR_SENSOR_IDX sensorIdx = IMGSENSOR_SENSOR_IDX_MAP(devId);

  if (sensorIdx < IMGSENSOR_SENSOR_IDX_MIN_NUM ||
      sensorIdx >= IMGSENSOR_SENSOR_IDX_MAX_NUM) {
    MY_LOGE("bad sensorDev:%#x", devId);
    return;
  }

  std::lock_guard<std::mutex> lk(m_sensorsLock);
  std::atomic_thread_fence(std::memory_order_acquire);

  auto itr = m_sensorsInfo.find(sensorIdx);
  if (CC_UNLIKELY(itr == m_sensorsInfo.end())) {
    MY_LOGE("static info not found (idx=%d,dev=%u)", sensorIdx, devId);
    return;
  }

  MY_LOGD("sensor info %u-th found: type=%u, deviceID=%u", itr->first,
          itr->second.type, itr->second.deviceId);

  ::memcpy(pSensorStaticInfo, &m_sensorsInfo[sensorIdx].info,
           sizeof(SensorStaticInfo));

  std::atomic_thread_fence(std::memory_order_release);
}

SensorStaticInfo const* IPCHalSensorListImpProv::querySensorStaticInfo(
    MUINT const index) const {  // Note: the argument "index" is the index of
                                // sensor (starts from 0).

  std::lock_guard<std::mutex> lk(m_sensorsLock);

  std::atomic_thread_fence(std::memory_order_acquire);
  //
  auto itr = m_sensorsInfo.find(index);
  if (CC_UNLIKELY(itr == m_sensorsInfo.end())) {
    MY_LOGE("static info not found (idx=%d)", index);
    return nullptr;
  }

  MY_LOGD("sensor info %u-th found: type=%u, deviceID=%u", itr->first,
          itr->second.type, itr->second.deviceId);

  return &itr->second.info;
}

IHalSensor* IPCHalSensorListImpProv::createSensor(char const* szCallerName,
                                                  MUINT const index) {
  std::lock_guard<std::mutex> lk(m_sensorsLock);
  std::atomic_thread_fence(std::memory_order_acquire);

  // if not exists, create one
  if (m_sensorsInfo[index].halSensor.get() == nullptr) {
    m_sensorsInfo[index].halSensor =
        std::shared_ptr<IHalSensor>(createIIPCSensor(index), [](IHalSensor* p) {
          if (p)
            p->destroyInstance(LOG_TAG);
        });
    std::atomic_thread_fence(std::memory_order_release);
  }

  return m_sensorsInfo[index].halSensor.get();
}

IHalSensor* IPCHalSensorListImpProv::createSensor(char const* szCallerName,
                                                  MUINT const uCountOfIndex,
                                                  MUINT const* pArrayOfIndex) {
  MY_LOGE("createSensor(const char*, MUINT const, MUINT const *) not impl");
  return nullptr;
}
