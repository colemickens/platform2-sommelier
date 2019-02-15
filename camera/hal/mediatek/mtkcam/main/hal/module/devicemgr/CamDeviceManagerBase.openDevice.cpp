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
//
#include <memory>
#include "MyUtils.h"
#include <mtkcam/main/hal/Cam3Device.h>
#include <mtkcam/utils/hw/CamManager.h>
#include <mtkcam/utils/std/Misc.h>
#include <property_lib.h>
#include <string>
//
using NSCam::CamDeviceManagerBase;
//
extern std::shared_ptr<NSCam::Cam3Device> createCam3Device(
    std::string const s8ClientAppMode, int32_t const i4OpenId);
/******************************************************************************
 *
 ******************************************************************************/
CamDeviceManagerBase::OpenInfo::~OpenInfo() {}

/******************************************************************************
 *
 ******************************************************************************/
CamDeviceManagerBase::OpenInfo::OpenInfo()
    : pDevice(0), uDeviceVersion(0), i8OpenTimestamp(0) {}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerBase::detachDeviceLocked(std::shared_ptr<ICamDevice> pDevice) {
  std::shared_ptr<OpenInfo> pOpenInfo;
  int32_t const openId = pDevice->getOpenId();
  //
  auto const index = mOpenMap.find(openId);
  pOpenInfo = index->second;
  if (index == mOpenMap.end() || !pOpenInfo || pOpenInfo->pDevice != pDevice) {
    MY_LOGE("device %d: not found!!! mOpenMap.size:%zu index:%zd pOpenInfo:%p",
            openId, mOpenMap.size(), std::distance(mOpenMap.begin(), index),
            pOpenInfo.get());
    MY_LOGE_IF(pOpenInfo, "device %p %p", pOpenInfo->pDevice.get(),
               pDevice.get());
    return NAME_NOT_FOUND;
  }
  //
  mOpenMap.erase(index);
  MY_LOGI("detach device: %s %d", pDevice->getDevName(), pDevice->getOpenId());

  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerBase::attachDeviceLocked(std::shared_ptr<ICamDevice> pDevice,
                                         uint32_t device_version) {
  int32_t const openId = pDevice->getOpenId();
  struct timeval t;
  //
  //
  auto iter = mOpenMap.find(openId);
  if (iter != mOpenMap.end()) {
    std::shared_ptr<ICamDevice> const pDev = iter->second->pDevice;
    MY_LOGE(
        "Busy deviceId:%d; device:%p has already been opend with version:0x%x "
        "OpenTimestamp:%" PRId64,
        openId, pDev.get(), iter->second->uDeviceVersion,
        iter->second->i8OpenTimestamp);
    MY_LOGE_IF(pDev != nullptr, "device: %s %d", pDev->getDevName(),
               pDev->getOpenId());

    return ALREADY_EXISTS;
  }
  //
  std::shared_ptr<OpenInfo> pOpenInfo = std::make_shared<OpenInfo>();
  pOpenInfo->pDevice = pDevice;
  pOpenInfo->uDeviceVersion = device_version;
  gettimeofday(&t, NULL);
  pOpenInfo->i8OpenTimestamp = (t.tv_sec) * 1000000000LL + (t.tv_usec) * 1000LL;
  //
  mOpenMap.emplace(openId, pOpenInfo);
  MY_LOGI("device: %s %d version:0x%x OpenTimestamp:%" PRId64,
          pDevice->getDevName(), pDevice->getOpenId(),
          pOpenInfo->uDeviceVersion, pOpenInfo->i8OpenTimestamp);
  mi4DeviceId = openId;
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerBase::validateOpenLocked(int32_t i4OpenId,
                                         uint32_t device_version) const {
  auto it = mEnumMap.find(i4OpenId);
  if (it == mEnumMap.end()) {
    MY_LOGE("Bad OpenId:%d - version:0x%x mEnumMap.size:%zu DeviceNum:%d",
            i4OpenId, device_version, mEnumMap.size(), mi4DeviceNum);
    return -EINVAL;
  }
  std::shared_ptr<EnumInfo> pEnumInfo = it->second;
  if (!pEnumInfo) {
    MY_LOGE("Bad OpenId:%d - version:0x%x mEnumMap.size:%zu DeviceNum:%d",
            i4OpenId, device_version, mEnumMap.size(), mi4DeviceNum);
    /*
     * -EINVAL:     The input arguments are invalid, i.e. the id is invalid,
     *              and/or the module is invalid.
     */
    return -EINVAL;
  }
  //
  auto iter = mOpenMap.find(i4OpenId);
  if (iter != mOpenMap.end()) {
    std::shared_ptr<ICamDevice> const pDev = iter->second->pDevice;
    MY_LOGE(
        "Busy deviceId:%d; device:%p has already been opend with version:0x%x "
        "OpenTimestamp:%" PRId64,
        i4OpenId, pDev.get(), iter->second->uDeviceVersion,
        iter->second->i8OpenTimestamp);
    MY_LOGE_IF(pDev != nullptr, "device: %s %d", pDev->getDevName(),
               pDev->getOpenId());
    /*
     * -EBUSY:      The camera device was already opened for this camera id
     *              (by using this method or common.methods->open method),
     *              regardless of the device HAL version it was opened as.
     */
    return -EBUSY;
  }
  //
  return validateOpenLocked(i4OpenId);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerBase::closeDeviceLocked(std::shared_ptr<ICamDevice> pDevice) {
  return detachDeviceLocked(pDevice);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerBase::openDeviceLocked(hw_device_t** device,
                                       hw_module_t const* module,
                                       int32_t const i4OpenId,
                                       uint32_t device_version) {
  MERROR status = OK;
  std::shared_ptr<ICamDevice> pDevice = nullptr;
  //
  std::string const s8ClientAppMode("cros_camera");
  //
  MY_LOGI("+ OpenId:%d with version 0x%x - mOpenMap.size:%zu mEnumMap.size:%zu",
          i4OpenId, device_version, mOpenMap.size(), mEnumMap.size());

  //
  //  [1] check to see whether it's ready to open.
  if (OK != (status = validateOpenLocked(i4OpenId, device_version))) {
    return status;
  }

  if (device_version >= CAMERA_DEVICE_API_VERSION_3_0) {
    pDevice = createCam3Device(s8ClientAppMode, i4OpenId);
  } else {
    MY_LOGE("Unsupported version:0x%x", device_version);
    return -EOPNOTSUPP;
  }
  //
  if (!pDevice) {
    MY_LOGE("device creation failure");
    return -ENODEV;
  }
  //
  //  [4] open device successfully.
  {
    std::shared_ptr<ICamDevice> pCamDevice(pDevice);
    *device = const_cast<hw_device_t*>(pDevice->get_hw_device());
    //
    pDevice->set_hw_module(module);
    pDevice->set_module_callbacks(mpModuleCallbacks);
    pDevice->setDeviceManager(this);
    //
    attachDeviceLocked(pCamDevice, device_version);
  }
  //
  return OK;
}
