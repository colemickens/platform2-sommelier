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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_IPCIHALSENSOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_IPCIHALSENSOR_H_

#include <mtkcam/def/common.h>
#include <mtkcam/drv/IHalSensor.h>

namespace NSCam {

//-----------------------------------------------------------------------------
// IIPCHalSensor:
//  use all APIs from IHalSensor, but additional IPC functions
//  Inheritance map: IPCIHalSensor --> IHalSensor
//                      ^
//                      |
//                  IPCHalSensorImp
//-----------------------------------------------------------------------------
class VISIBILITY_PUBLIC IIPCHalSensor : public IHalSensor {
 public:
  struct DynamicInfo {
    MSize bin_size;
    MSize hbin_size;
    MSize tg_size;
    MUINT8 tg_info;
    //
    //
    DynamicInfo()
        : bin_size(MSize(0, 0)),
          hbin_size(MSize(0, 0)),
          tg_size(MSize(0, 0)),
          tg_info(0) {}
  } __attribute__((packed));

 public:
  // All information are sent by this method.
  virtual void ipcSetDynamicInfo(const SensorDynamicInfo& info) = 0;

  // Extended dynamic info
  virtual void ipcSetDynamicInfoEx(const DynamicInfo& info) = 0;

  // Retrieve extended dynamic info
  virtual DynamicInfo getDynamicInfoEx() const = 0;

  // update command for the result of IHalSensor::sendCommand since
  // IIPCHalSensor cannot access driver.
  virtual void updateCommand(MUINT index,
                             MUINTPTR cmd,
                             MUINTPTR arg1,
                             MUINTPTR arg2,
                             MUINTPTR arg3) = 0;

 protected:
  static IIPCHalSensor* createInstance(int index);

  // Caller has to invoke IHalSensor::destroyInstance to release the
  // IIPCHalSensor resource.
  virtual ~IIPCHalSensor() = default;

 private:
  friend class IHalSensor;
  friend class IIPCHalSensorListProv;
};

//-----------------------------------------------------------------------------
// IIPCHalSensorList:
//  use all APIs from IHalSensorList, but additional IPC functions
//  Inheritance map: IIPCHalSensorList --> IHalSensorList
//                          ^
//                          |
//                  IPCHalSensorListImp
//-----------------------------------------------------------------------------
class VISIBILITY_PUBLIC IIPCHalSensorListProv : public IHalSensorList {
 public:
  static IIPCHalSensorListProv* getInstance();

 protected:
  static IIPCHalSensor* createIIPCSensor(int index);

 public:
  virtual void ipcSetSensorStaticInfo(MUINT32 idx,
                                      MUINT32 type,
                                      MUINT32 deviceId,
                                      const SensorStaticInfo& info) = 0;
  virtual void ipcSetStaticInfo(MUINT32 idx, const IMetadata& info) = 0;
};

class VISIBILITY_PUBLIC IIPCHalSensorList : public IHalSensorList {
 public:
  static IIPCHalSensorList* getInstance();

 protected:
  static IIPCHalSensor* createIIPCSensor();

 public:
  virtual void ipcSetSensorStaticInfo(MUINT32 idx,
                                      MUINT32 type,
                                      MUINT32 deviceId,
                                      const SensorStaticInfo& info) = 0;

  virtual void ipcSetStaticInfo(MUINT32 idx, const IMetadata& info) = 0;
};

};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_IPCIHALSENSOR_H_
