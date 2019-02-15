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

#undef LOG_TAG
#define LOG_TAG "mtkcam-DeviceAdapter"

#include <future>
#include <impl/IHalDeviceAdapter.h>
#include <memory>
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/utils/LogicalCam/IHalLogicalDeviceList.h>
#include <string>
#include <sys/prctl.h>
#include <vector>
#include "IHal3AAdapter.h"
#include "MyUtils.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {
class HalDeviceAdapter : public IHalDeviceAdapter {
  int32_t mId;  // logical device ID
  std::string const mName;

  // init
  std::vector<int32_t> mvPhySensorId;  // physical sensor ID (index)

  // open/close
  bool mIsOpen;

  // powerOn/powerOff
  std::vector<std::shared_ptr<NSCam::IHalSensor>> mvHalSensor;
  std::vector<std::shared_ptr<IHal3AAdapter>> mvHal3A;

 public:  ////    Operations.
  explicit HalDeviceAdapter(int32_t id)
      : mId(id),
        mName("HalDeviceAdapter:" + std::to_string(id)),
        mIsOpen(false) {
    MY_LOGD("%p %s", this, mName.c_str());
  }
  ~HalDeviceAdapter() {}

  virtual auto init() -> bool {
    auto pHalDeviceList = MAKE_HalLogicalDeviceList();
    if (CC_UNLIKELY(pHalDeviceList == nullptr)) {
      MY_LOGE("Bad pHalDeviceList");
      return false;
    }
    //
    mvPhySensorId = pHalDeviceList->getSensorIds(mId);
    return true;
  }

  auto open() -> bool override {
    CAM_TRACE_NAME("Sensor creation");
    auto pHalSensorList = GET_HalSensorList();
    if (CC_UNLIKELY(pHalSensorList == nullptr)) {
      MY_LOGE("Bad HalSensorList");
      return false;
    }
    //
    bool ret = true;
    for (size_t i = 0; i < mvPhySensorId.size(); i++) {
      auto deleter = [&](IHalSensor* p_halsensor) {
        if (CC_LIKELY(p_halsensor != nullptr)) {
          p_halsensor->destroyInstance(mName.c_str());
        }
      };
      std::shared_ptr<NSCam::IHalSensor> pSensor;
      pSensor.reset(
          pHalSensorList->createSensor(mName.c_str(), mvPhySensorId[i]),
          deleter);
      mvHalSensor.push_back(pSensor);
      if (CC_UNLIKELY(pSensor == nullptr)) {
        ret = false;
        MY_LOGE("Bad HalSensor - mvPhySensorId[%zu]=%d", i, mvPhySensorId[i]);
      }
    }

    if (CC_UNLIKELY(!ret)) {
      MY_LOGE("Fail on open().let's clean up resources");
      mvHalSensor.clear();
    }
    mIsOpen = ret;
    return ret;
  }

  auto close() -> void override {
    CAM_TRACE_NAME("Sensor destruction");
    mvHalSensor.clear();
    mvHal3A.clear();
    mIsOpen = false;
  }

  auto powerOn() -> bool override {
    CAM_TRACE_NAME("LogicalDev powerOn");
    NSCam::Utils::CamProfile profile(__FUNCTION__, mName.c_str());
    if (CC_UNLIKELY(!mIsOpen)) {
      MY_LOGE("Bad HalSensor");
      return false;
    }
    // create thread to power on sensors
    std::future<bool> future_initSensor =
        std::async(std::launch::async, [this]() {
          CAM_TRACE_NAME("Sensors powerOn");
          ::prctl(PR_SET_NAME, (uint64_t) "LogicalDevPoweron", 0, 0, 0);
          //
          for (size_t i = 0; i < mvPhySensorId.size(); i++) {
            MUINT const sensorIndex = mvPhySensorId[i];
            if ((mvHalSensor[i] == nullptr) ||
                (!mvHalSensor[i]->powerOn(mName.c_str(), 1, &sensorIndex))) {
              return false;
            }
          }
          //
          return true;
        });
    // init 3A and poweron 3A
    bool success_sensorPowerOn = false;
    bool success_init3A = true;
    for (size_t i = 0; i < mvPhySensorId.size(); i++) {
      mvHal3A.push_back(IHal3AAdapter::create(mvPhySensorId[i], mName.c_str()));
      profile.print("3A Hal -");
    }

    // (3) Wait for Sensor
    {
      success_sensorPowerOn = future_initSensor.get();
      if (!success_sensorPowerOn) {
        return false;
      }
      profile.print("Sensor powerOn -");
    }

    // (4) Notify 3A of Power On
    for (size_t i = 0; i < mvHal3A.size(); i++) {
      if (mvHal3A[i] != nullptr) {
        mvHal3A[i]->notifyPowerOn();
      } else {
        success_init3A = false;
        break;
      }
    }
    profile.print("3A notifyPowerOn -");

    return (success_init3A && success_sensorPowerOn);
  }

  auto powerOff() -> void override {
    CAM_TRACE_NAME("LogicalDev powerOff");
    for (size_t i = 0; i < mvPhySensorId.size(); i++) {
      MUINT const sensorIndex = mvPhySensorId[i];
      if (mvHal3A[i] != nullptr)
        mvHal3A[i]->notifyPowerOff();
      if (mvHalSensor[i] != nullptr)
        mvHalSensor[i]->powerOff(mName.c_str(), 1, &sensorIndex);
    }
  }

  auto getPhysicalSensorId(std::vector<int32_t>* rvSensorId) const
      -> bool override {
    if (mvPhySensorId.empty()) {
      return false;
    }
    *rvSensorId = mvPhySensorId;
    return true;
  }
};

/******************************************************************************
 *
 ******************************************************************************/
auto IHalDeviceAdapter::create(int32_t id)
    -> std::shared_ptr<IHalDeviceAdapter> {
  auto p = std::make_shared<HalDeviceAdapter>(id);

  if (CC_UNLIKELY(!p->init())) {
    return nullptr;
  }
  return p;
}

};  // namespace model
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
