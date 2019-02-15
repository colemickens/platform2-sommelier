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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_INCLUDE_CAMDEVICEMANAGERBASE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_INCLUDE_CAMDEVICEMANAGERBASE_H_

#include <memory>
#include <string.h>
#include <unordered_map>
#include <mtkcam/main/hal/ICamDeviceManager.h>
#include <mtkcam/main/hal/ICamDevice.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *
 ******************************************************************************/
class CamDeviceManagerBase : public ICamDeviceManager {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                        Enum Info.
  class EnumInfo {
   public:                    ////                    fields.
    uint32_t uDeviceVersion;  //  Device Version (CAMERA_DEVICE_API_VERSION_X_X)
    camera_metadata const* pMetadata;  //  Device Metadata.
    int32_t iFacing;                   //  Device Facing Direction.
    int32_t iWantedOrientation;        //  Device Wanted Orientation.
    int32_t iSetupOrientation;         //  Device Setup Orientation.
    int32_t iHasFlashLight;            //  Device Flash Light.
   public:                             ////                    operations.
    ~EnumInfo();
    EnumInfo();
  };

  typedef std::unordered_map<int32_t, std::shared_ptr<EnumInfo> > EnumInfoMap_t;

 protected:  ////                        Open Info.
  class OpenInfo {
   public:  ////                    fields.
    std::shared_ptr<ICamDevice> pDevice;
    uint32_t uDeviceVersion;
    int64_t i8OpenTimestamp;

   public:  ////                    operations.
    ~OpenInfo();
    OpenInfo();
  };

  typedef std::unordered_map<int32_t, std::shared_ptr<OpenInfo> > OpenInfoMap_t;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                        Data Members.
  pthread_rwlock_t mRWLock;
  void* mpLibPlatform;
  camera_module_callbacks_t const* mpModuleCallbacks;
  int32_t mi4DeviceNum;
  int32_t mi4DeviceId;
  EnumInfoMap_t mEnumMap;
  OpenInfoMap_t mOpenMap;
  vendor_tag_ops_t mVendorTagOps;

 public:  ////                        Instantiation.
  virtual ~CamDeviceManagerBase();
  CamDeviceManagerBase();

 protected:  ////                        Operations.
  virtual MERROR attachDeviceLocked(std::shared_ptr<ICamDevice> pDevice,
                                    uint32_t device_version);
  virtual MERROR detachDeviceLocked(std::shared_ptr<ICamDevice> pDevice);

  virtual MERROR closeDeviceLocked(std::shared_ptr<ICamDevice> pDevice);
  virtual MERROR openDeviceLocked(hw_device_t** device,
                                  hw_module_t const* module,
                                  int32_t const i4OpenId,
                                  uint32_t device_version);

  virtual MERROR validateOpenLocked(int32_t /*i4OpenId*/) const { return OK; }

  virtual MERROR validateOpenLocked(int32_t i4OpenId,
                                    uint32_t device_version) const;

  virtual int32_t enumDeviceLocked() = 0;

  virtual std::shared_ptr<OpenInfo> getOpenInfo(int const deviceId) {
    auto find = mOpenMap.find(deviceId);
    if (find == mOpenMap.end())
      return nullptr;
    else
      return find->second;
  }
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  ICamDeviceManager Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  virtual MERROR open(hw_device_t** device,
                      hw_module_t const* module,
                      char const* name,
                      uint32_t device_version);

  virtual MERROR close();

  virtual int32_t getNumberOfDevices();

  virtual MERROR getDeviceInfo(int deviceId, camera_info* rInfo);

  virtual MERROR setCallbacks(camera_module_callbacks_t const* callbacks);
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_INCLUDE_CAMDEVICEMANAGERBASE_H_
