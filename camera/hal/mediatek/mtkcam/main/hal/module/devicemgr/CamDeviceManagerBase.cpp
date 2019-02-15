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
#include <mtkcam/utils/metadata/client/mtk_metadata_tag.h>

using NSCam::CamDeviceManagerBase;
/******************************************************************************
 *
 ******************************************************************************/
CamDeviceManagerBase::~CamDeviceManagerBase() {
  if (mpLibPlatform) {
    ::dlclose(mpLibPlatform);
    mpLibPlatform = NULL;
  }
  pthread_rwlock_destroy(&mRWLock);
}

/******************************************************************************
 *
 ******************************************************************************/
CamDeviceManagerBase::CamDeviceManagerBase()
    : ICamDeviceManager(),
      mpLibPlatform(NULL),
      mpModuleCallbacks(NULL),
      mi4DeviceNum(0) {
  pthread_rwlock_init(&mRWLock, NULL);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerBase::open(hw_device_t** device,
                           hw_module_t const* module,
                           char const* name,
                           uint32_t device_version) {
  int32_t i4OpenId = (name != NULL) ? ::atoi(name) : -1;

  MY_LOGI("mtk CamDeviceManagerBase:open, openid:%d, version:0x%x", i4OpenId,
          device_version);

  if (0 == device_version) {
    camera_info info;
    MERROR status;
    if (OK != (status = getDeviceInfo(i4OpenId, &info))) {
      return status;
    }
    device_version = info.device_version;
    MY_LOGI("adjust, version:0x%x", device_version);
  }
  //
  pthread_rwlock_wrlock(&mRWLock);
  MERROR ret = openDeviceLocked(device, module, i4OpenId, device_version);
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerBase::close() {
  pthread_rwlock_wrlock(&mRWLock);
  //
  auto info = getOpenInfo(mi4DeviceId);
  if (info == nullptr) {
    MY_LOGE("device %d: not found!!! mOpenMap.size:%zu ", mi4DeviceId,
            mOpenMap.size());
    return NAME_NOT_FOUND;
  }
  std::shared_ptr<ICamDevice> pDevice = info->pDevice;
  MERROR ret = closeDeviceLocked(pDevice);
  pthread_rwlock_unlock(&mRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t CamDeviceManagerBase::getNumberOfDevices() {
  pthread_rwlock_wrlock(&mRWLock);
  //
  if (0 != mi4DeviceNum) {
    MY_LOGI("#devices:%d", mi4DeviceNum);
  } else {
    Utils::CamProfile _profile(__FUNCTION__, "CamDeviceManagerBase");
    mi4DeviceNum = enumDeviceLocked();
    _profile.print("");
  }
  //
  pthread_rwlock_unlock(&mRWLock);
  return mi4DeviceNum;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerBase::getDeviceInfo(int deviceId, camera_info* rInfo) {
  pthread_rwlock_rdlock(&mRWLock);
  //
  auto iter = mEnumMap.find(deviceId);
  if (iter == mEnumMap.end()) {
    pthread_rwlock_unlock(&mRWLock);
    MY_LOGE("Bad deviceId:%d", deviceId);
    return -EINVAL;
  }
  switch (iter->second->iFacing) {
    case MTK_LENS_FACING_FRONT:
      rInfo->facing = CAMERA_FACING_FRONT;
      break;
    case MTK_LENS_FACING_BACK:
      rInfo->facing = CAMERA_FACING_BACK;
      break;
    case MTK_LENS_FACING_EXTERNAL:
      rInfo->facing = CAMERA_FACING_EXTERNAL;
      break;
    default:
      MY_LOGE("Unknown facing type:%d", iter->second->iFacing);
      break;
  }
  //
  rInfo->device_version = iter->second->uDeviceVersion;
  rInfo->orientation = iter->second->iWantedOrientation;
  rInfo->static_camera_characteristics = iter->second->pMetadata;
  //
  rInfo->resource_cost = 0;
  rInfo->conflicting_devices = 0;
  rInfo->conflicting_devices_length = 0;
  //
  MY_LOGI("deviceId:%d device_version:0x%x facing:%d orientation:%d", deviceId,
          rInfo->device_version, rInfo->facing, rInfo->orientation);
  pthread_rwlock_unlock(&mRWLock);
  return OK;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
CamDeviceManagerBase::setCallbacks(camera_module_callbacks_t const* callbacks) {
  pthread_rwlock_wrlock(&mRWLock);
  //
  mpModuleCallbacks = callbacks;
  pthread_rwlock_unlock(&mRWLock);
  return OK;
}
