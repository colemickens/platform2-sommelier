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

#define LOG_TAG "MtkCam/Utils/CamMgr"
//
#include <dlfcn.h>
#include <fcntl.h>
#include <mtkcam/utils/hw/CamManager.h>
#include <mtkcam/utils/std/Log.h>
#include <mutex>
#include <stdlib.h>
#include <unistd.h>
//

using NSCam::Utils::CamManager;
/******************************************************************************
 *
 ******************************************************************************/
CamManager* CamManager::getInstance() {
  static CamManager* tmp = new CamManager();
  return tmp;
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::incDevice(int32_t openId) {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  for (uint32_t i = 0; i < mvOpenId.size(); i++) {
    if (mvOpenId[i] == openId) {
      MY_LOGW("openId %d is already exist", openId);
      return;
    }
  }
  //
  MY_LOGD("openId %d", openId);
  mvOpenId.push_back(openId);
  if (mvOpenId.size() == 2) {
    MY_LOGD("enable termal policy");
    setThermalPolicy("thermal_policy_01", 1);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::decDevice(int32_t openId) {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  for (uint32_t i = 0; i < mvOpenId.size(); i++) {
    if (mvOpenId[i] == openId) {
      mvOpenId.erase(mvOpenId.begin() + i);
      MY_LOGD("openId %d", openId);
      if (mvOpenId.size() == 1) {
        MY_LOGD("disable termal policy");
        setThermalPolicy("thermal_policy_01", 0);
      }
      return;
    }
  }
  MY_LOGW("openId %d is not Found", openId);
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t CamManager::getFirstOpenId() const {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  int32_t openId = -1;
  if (mvOpenId.size() > 0) {
    openId = mvOpenId[0];
  }
  MY_LOGD("openId %d", openId);
  return openId;
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::incUsingDevice(int32_t openId) {
  {
    std::unique_lock<std::mutex> _l(mLockMtx);
    //
    for (uint32_t i = 0; i < mvUsingId.size(); i++) {
      MY_LOGD("openId %d-%d", i, mvUsingId[i]);
      if (mvUsingId[i] == openId) {
        MY_LOGW("openId %d is already in use", openId);
        return;
      }
    }
    //
    MY_LOGD("openId %d", openId);
    mvUsingId.push_back(openId);
  }
  {
    std::unique_lock<std::mutex> _l(mLockConfig);
    MY_LOGD("%d is in config", mDeviceInConfig);
    if (mDeviceInConfig >= 0) {
      mConfigCond.wait(_l);
      mDeviceInConfig = openId;
    } else {
      mDeviceInConfig = openId;
    }
  }
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::configUsingDeviceDone(int32_t openId) {
  std::lock_guard<std::mutex> _l(mLockConfig);
  MY_LOGD("get mDeviceInConfig(%d), openId(%d)", mDeviceInConfig, openId);
  if (mDeviceInConfig != openId) {
    MY_LOGW("openId(%d), inConfigId(%d), doesn't unlock mConfigCond", openId,
            mDeviceInConfig);
    return;
  }
  mDeviceInConfig = -1;
  mConfigCond.notify_one();
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::decUsingDevice(int32_t openId) {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  for (uint32_t i = 0; i < mvUsingId.size(); i++) {
    if (mvUsingId[i] == openId) {
      mvUsingId.erase(mvUsingId.begin() + i);
      MY_LOGD("openId %d", openId);
      return;
    }
  }
  MY_LOGW("openId %d is not Found", openId);
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t CamManager::getFirstUsingId() const {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  int32_t openId = -1;
  if (mvUsingId.size() > 0) {
    openId = mvUsingId[0];
  }
  MY_LOGD("openId %d", openId);
  return openId;
}

/******************************************************************************
 *
 ******************************************************************************/
uint32_t CamManager::getDeviceCount() const {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  return mvOpenId.size();
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::setFrameRate(uint32_t const id, uint32_t const frameRate) {
  std::lock_guard<std::mutex> _l(mLockFps);
  //
  if (id == 0) {
    mFrameRate0 = frameRate;
  } else {
    MY_LOGE("id(%d), frameRate(%d)", id, frameRate);
  }
}

/******************************************************************************
 *
 ******************************************************************************/
uint32_t CamManager::getFrameRate(uint32_t const id) const {
  std::lock_guard<std::mutex> _l(mLockFps);
  //
  uint32_t frameRate;
  if (id == 0) {
    frameRate = mFrameRate0;
  } else {
    MY_LOGE("error ,only support id(%d)", id);
  }

  return (mvOpenId.size() > id) ? frameRate : 0;
}

/******************************************************************************
 *
 ******************************************************************************/
bool CamManager::isMultiDevice() const {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  return (mvOpenId.size() > 1) ? true : false;
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::setRecordingHint(bool const isRecord) {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  mbRecord = isRecord;
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::setAvailableHint(bool const isAvailable) {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  mbAvailable = isAvailable;
}

/******************************************************************************
 *
 ******************************************************************************/
bool CamManager::getPermission() const {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  MY_LOGD(
      "OpenId.size(%zu), mbRecord(%d), mbAvailable(%d), 0:fps(%d); 1:fps(%d)",
      mvOpenId.size(), mbRecord, mbAvailable, getFrameRate(0), getFrameRate(1));
  return !mbRecord && mbAvailable;
}
/******************************************************************************
 *
 ******************************************************************************/
void CamManager::incSensorCount(const char* caller) {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  mSensorPowerCount++;
  MY_LOGD("[%s] current sensor count [%d]", caller, mSensorPowerCount);
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::decSensorCount(const char* caller) {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  mSensorPowerCount--;
  MY_LOGD("[%s] current sensor count [%d]", caller, mSensorPowerCount);
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t CamManager::getSensorCount() const {
  std::lock_guard<std::mutex> _l(mLockMtx);
  //
  return mSensorPowerCount;
}

/******************************************************************************
 *
 ******************************************************************************/
bool CamManager::setThermalPolicy(const char* policy, bool usage) {
  MY_LOGD("policy(%s) usage(%d) +", policy, usage);

  void* handle = dlopen("/system/vendor/lib/libmtcloader.so", RTLD_NOW);
  if (NULL == handle) {
    MY_LOGW("%s, can't load thermal library: %s", __FUNCTION__, dlerror());
    return false;
  }
  typedef int (*load_change_policy)(const char*, int);
  MY_LOGD("dl sym");
  void* func = dlsym(handle, "change_policy");

  if (NULL != func) {
    load_change_policy change_policy =
        reinterpret_cast<load_change_policy>(func);

    MY_LOGD("change policy");
    change_policy(policy, usage);
  } else {
    MY_LOGW("dlsym fail!");
  }

  MY_LOGD("dl close");
  dlclose(handle);

  MY_LOGD("policy(%s) usage(%d) -", policy, usage);
  return true;
}

/******************************************************************************
 *
 ******************************************************************************/
void CamManager::getStartPreviewLock() {
  MY_LOGI("+");
  mLockMtx.lock();
  while (mLockStartPreview.try_lock() != true) {
    mLockMtx.unlock();
    usleep(10 * 1000);
    mLockMtx.lock();
  }
  mLockMtx.unlock();
  MY_LOGI("-");
}

void CamManager::releaseStartPreviewLock() {
  MY_LOGI("+");
  std::lock_guard<std::mutex> _l(mLockMtx);
  mLockStartPreview.unlock();
  MY_LOGI("-");
}
