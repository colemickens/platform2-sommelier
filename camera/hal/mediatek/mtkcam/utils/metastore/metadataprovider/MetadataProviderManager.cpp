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

#define LOG_TAG "MtkCam/Metadata"

#include <map>
#include <memory>

#include <mtkcam/utils/std/Log.h>
#include <mtkcam/utils/metastore/IMetadataProvider.h>
#include <mtkcam/utils/LogicalCam/IHalLogicalDeviceList.h>

namespace NSCam {
namespace NSMetadataProviderManager {
/******************************************************************************
 *
 ******************************************************************************/
namespace {
typedef std::map<int32_t, std::shared_ptr<IMetadataProvider> > Map_t;
Map_t gMap;
pthread_rwlock_t gRWLock = PTHREAD_RWLOCK_INITIALIZER;
};  // namespace

/******************************************************************************
 *
 ******************************************************************************/
void clear() {
  pthread_rwlock_wrlock(&gRWLock);
  gMap.clear();
  pthread_rwlock_unlock(&gRWLock);
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t add(int32_t deviceId, std::shared_ptr<IMetadataProvider> pProvider) {
  pthread_rwlock_wrlock(&gRWLock);
  ssize_t index = 0;
  gMap.emplace(deviceId, pProvider);
  index = gMap.size() - 1;
  CAM_LOGD("[%zd] deviceId:%d -> %p", index, deviceId, pProvider.get());
  pthread_rwlock_unlock(&gRWLock);
  return index;
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t remove(int32_t deviceId) {
  pthread_rwlock_wrlock(&gRWLock);
  ssize_t index = gMap.erase(deviceId);
  CAM_LOGD("[%zd] deviceId:%d removed", index, deviceId);
  pthread_rwlock_unlock(&gRWLock);
  return index;
}

int32_t convertId(int32_t SensorId) {
  IHalLogicalDeviceList* pHalDeviceList;
  pHalDeviceList = MAKE_HalLogicalDeviceList();
  return pHalDeviceList->getDeviceId(SensorId);
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IMetadataProvider> valueForByDeviceId(int32_t deviceId) {
  pthread_rwlock_rdlock(&gRWLock);
  if (gMap.find(deviceId) == gMap.end()) {
    pthread_rwlock_unlock(&gRWLock);
    return NULL;
  }
  auto p = gMap.at(deviceId);
  pthread_rwlock_unlock(&gRWLock);
  return p;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IMetadataProvider> valueFor(int32_t sensorId) {
  pthread_rwlock_rdlock(&gRWLock);
  if (gMap.find(convertId(sensorId)) == gMap.end()) {
    pthread_rwlock_unlock(&gRWLock);
    return NULL;
  }
  auto p = gMap.at(convertId(sensorId));
  pthread_rwlock_unlock(&gRWLock);
  return p;
}

/******************************************************************************
 *
 ******************************************************************************/
std::shared_ptr<IMetadataProvider> valueAt(size_t index) {
  pthread_rwlock_rdlock(&gRWLock);
  if (index >= gMap.size()) {
    pthread_rwlock_unlock(&gRWLock);
    return NULL;
  }
  auto iter = gMap.begin();
  std::advance(iter, index);
  auto p = iter->second;
  pthread_rwlock_unlock(&gRWLock);
  return p;
}

/******************************************************************************
 *
 ******************************************************************************/
int32_t keyAt(size_t index) {
  pthread_rwlock_rdlock(&gRWLock);
  if (index >= gMap.size()) {
    return NULL;
  }
  auto iter = gMap.begin();
  std::advance(iter, index);

  auto ret = iter->first;
  pthread_rwlock_unlock(&gRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
ssize_t indexOfKey(int32_t sensorId) {
  pthread_rwlock_rdlock(&gRWLock);
  auto iter = gMap.find(convertId(sensorId));
  if (iter == gMap.end()) {
    pthread_rwlock_unlock(&gRWLock);
    return -1;
  }
  auto ret = std::distance(gMap.begin(), iter);
  pthread_rwlock_unlock(&gRWLock);
  return ret;
}

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace NSMetadataProviderManager
};  // namespace NSCam
