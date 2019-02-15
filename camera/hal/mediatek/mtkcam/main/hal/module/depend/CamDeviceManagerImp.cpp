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

#define LOG_TAG "MtkCam/devicemgr"

#include <mtkcam/main/hal/module/depend/CamDeviceManagerImp.h>
//
#include <memory>
#include <stdio.h>
#include "MyUtils.h"
//
#include <mtkcam/utils/hw/CamManager.h>
#include <mtkcam/utils/LogicalCam/IHalLogicalDeviceList.h>
//
#include <mtkcam/drv/IHalSensor.h>
#include <mtkcam/ipc/client/Mediatek3AClient.h>
//
using NSCam::CamDeviceManagerImp;
using NSCam::Utils::CamManager;
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
ICamDeviceManager* getCamDeviceManager() {
  static CamDeviceManagerImp* instance = new CamDeviceManagerImp();
  return instance;
}
}  // namespace NSCam

/******************************************************************************
 *
 ******************************************************************************/
CamDeviceManagerImp::CamDeviceManagerImp() : CamDeviceManagerBase() {}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerImp::validateOpenLocked(int32_t i4OpenId,
                                        uint32_t device_version) const {
  MERROR status = OK;
  //
  status = CamDeviceManagerBase::validateOpenLocked(i4OpenId, device_version);
  if (OK != status) {
    return status;
  }
  //
  if (0 != mOpenMap.size()) {
    MY_LOGE("[Now] fail to open (deviceId:%d version:0x%x) => failure",
            i4OpenId, device_version);
    MY_LOGE("[Previous] (deviceId:%d version:0x%x) mOpenMap.size:%zu",
            mOpenMap.begin()->first, mOpenMap.begin()->second->uDeviceVersion,
            mOpenMap.size());
    return -EUSERS;
  }
  //
  if (!CamManager::getInstance()->getPermission()) {
    MY_LOGE("Cannot open device %d ... Permission denied", i4OpenId);
    return -EUSERS;
  }
  //

  if (Mediatek3AClient::getInstance(i4OpenId) &&
      !Mediatek3AClient::getInstance(i4OpenId)->isIPCFine()) {
    Mediatek3AClient::getInstance(i4OpenId)->tryReconnectBridge();

    if (!Mediatek3AClient::getInstance(i4OpenId)->isIPCFine()) {
      MY_LOGE("Reconnect IPC fail, cannot open device %d ...", i4OpenId);
      return -EUSERS;
    }
  }

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t CamDeviceManagerImp::enumDeviceLocked() {
  int32_t i4DeviceNum = 0;
  //
#if '1' == MTKCAM_HAVE_METADATA
  NSMetadataProviderManager::clear();
#endif
  mEnumMap.clear();
  //----------------------------------------------------------------------------
#if '1' == MTKCAM_HAVE_SENSOR_HAL
  //
  IHalLogicalDeviceList* pHalDeviceList;
  pHalDeviceList = MAKE_HalLogicalDeviceList();
  size_t const deviceNum = pHalDeviceList->searchDevices();
  ///
  CAM_LOGI("pLogicHalDeviceList:%p searchDevices:%zu queryNumberOfDevices:%d",
           pHalDeviceList, deviceNum, pHalDeviceList->queryNumberOfDevices());

  for (size_t instanceId = 0; instanceId < deviceNum; instanceId++) {
    //
    std::shared_ptr<IMetadataProvider> pMetadataProvider;
    pMetadataProvider = IMetadataProvider::create(instanceId);
    NSMetadataProviderManager::add(instanceId, pMetadataProvider);
    MY_LOGD("[0x%02zx] IMetadataProvider:%p sensor:%s", instanceId,
            pMetadataProvider.get(),
            pHalDeviceList->queryDriverName(instanceId));
  }

  size_t const sensorNum = pHalDeviceList->queryNumberOfSensors();
  CAM_LOGI("sensorNum:%d", sensorNum);
  mEnumMap.reserve(sensorNum + 1);
  for (size_t sensorId = 0; sensorId < sensorNum; sensorId++) {
    std::shared_ptr<EnumInfo> pInfo = std::make_shared<EnumInfo>();
    std::shared_ptr<IMetadataProvider> pMetadataProvider =
        NSMetadataProviderManager::valueFor(sensorId);
    pInfo->uDeviceVersion = pMetadataProvider->getDeviceVersion();
    pInfo->pMetadata = pMetadataProvider->getStaticCharacteristics();
    pInfo->iFacing = pMetadataProvider->getDeviceFacing();
    pInfo->iWantedOrientation = pMetadataProvider->getDeviceWantedOrientation();
    pInfo->iSetupOrientation = pMetadataProvider->getDeviceSetupOrientation();
    pInfo->iHasFlashLight = pMetadataProvider->getDeviceHasFlashLight();

    mEnumMap.emplace(sensorId, pInfo);
    i4DeviceNum++;
  }
  MY_LOGI("i4DeviceNum=%d", i4DeviceNum);
  for (auto const& it : mEnumMap) {
    int32_t const deviceId = it.first;
    std::shared_ptr<EnumInfo> pInfo = it.second;
    MY_LOGI(
        "[0x%02x] DeviceVersion:0x%x metadata:%p facing:%d"
        " orientation(wanted/setup)=(%d/%d)",
        deviceId, pInfo->uDeviceVersion, pInfo->pMetadata, pInfo->iFacing,
        pInfo->iWantedOrientation, pInfo->iSetupOrientation);
  }
  //----------------------------------------------------------------------------
#else  // #if '1'==MTKCAM_HAVE_SENSOR_HAL
  //----------------------------------------------------------------------------

#warning "[WARN] Simulation for CamDeviceManagerImp::enumDeviceLocked()"

  {
    int32_t const deviceId = 0;
    //
    std::shared_ptr<EnumInfo> pInfo = std::make_shared<EnumInfo>();
    mEnumMap.add(deviceId, pInfo);
    //
#if '1' == MTKCAM_HAVE_METADATA
    std::shared_ptr<IMetadataProvider> pMetadataProvider =
        IMetadataProvider::create(deviceId);
    if (pMetadataProvider == nullptr) {
      MY_LOGE("[%d] IMetadataProvider::create", deviceId);
    }
    NSMetadataProviderManager::add(deviceId, pMetadataProvider.get());
    //
    pInfo->uDeviceVersion = pMetadataProvider->getDeviceVersion();
    pInfo->pMetadata = pMetadataProvider->getStaticCharacteristics();
    pInfo->iFacing = pMetadataProvider->getDeviceFacing();
    pInfo->iWantedOrientation = pMetadataProvider->getDeviceWantedOrientation();
    pInfo->iSetupOrientation = pMetadataProvider->getDeviceSetupOrientation();
#endif
    //
    i4DeviceNum++;
  }
  //
  // mATV
  {
    int32_t const deviceId = 0xFF;
    //
    std::shared_ptr<EnumInfo> pInfo = std::make_shared<EnumInfo>;
    mEnumMap.add(deviceId, pInfo);
    pInfo->uDeviceVersion = CAMERA_DEVICE_API_VERSION_1_0;
    pInfo->pMetadata = NULL;
    pInfo->iFacing = 0;
    pInfo->iWantedOrientation = 0;
    pInfo->iSetupOrientation = 0;
  }
#endif  // #if '1'==MTKCAM_HAVE_SENSOR_HAL
  //----------------------------------------------------------------------------
  //
  return i4DeviceNum;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerImp::attachDeviceLocked(std::shared_ptr<ICamDevice> pDevice,
                                        uint32_t device_version) {
  MERROR status = OK;
  //
  status = CamDeviceManagerBase::attachDeviceLocked(pDevice, device_version);
  if (OK != status) {
    return status;
  }
  //
  CamManager* pCamMgr = CamManager::getInstance();
  pCamMgr->incDevice(pDevice->getOpenId());
  //
  return status;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerImp::detachDeviceLocked(std::shared_ptr<ICamDevice> pDevice) {
  MERROR status = OK;
  //
  status = CamDeviceManagerBase::detachDeviceLocked(pDevice);
  if (OK != status) {
    return status;
  }
  //
  CamManager* pCamMgr = CamManager::getInstance();
  pCamMgr->decDevice(pDevice->getOpenId());
  //
  return status;
}
