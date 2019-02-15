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

#define LOG_TAG "MtkCam/Cam3Device"
//
#include <string>
#include "MyUtils.h"
#include "Cam3DeviceBase.h"
//
using NSCam::Cam3DeviceBase;
/******************************************************************************
 *
 ******************************************************************************/
Cam3DeviceBase::Cam3DeviceBase(std::string const& rDevName,
                               int32_t const i4OpenId)
    : mpDeviceManager(NULL),
      mDevName(rDevName),
      mi4OpenId(i4OpenId),
      mpCallbackOps(NULL) {
  char cLogLevel[PROPERTY_VALUE_MAX] = {0};
  property_get("debug.camera.log", cLogLevel, "0");
  mLogLevel = ::atoi(cLogLevel);
  if (mLogLevel == 0) {
    property_get("debug.camera.log.Cam3Device", cLogLevel, "0");
    mLogLevel = ::atoi(cLogLevel);
  }
  MY_LOGD("LogLevel:%d", mLogLevel);
}

/******************************************************************************
 *
 ******************************************************************************/
Cam3DeviceBase::~Cam3DeviceBase() {}

/******************************************************************************
 *
 ******************************************************************************/
MINT32
Cam3DeviceBase::getLogLevel() const {
  return mLogLevel;
}

/******************************************************************************
 *
 ******************************************************************************/
void Cam3DeviceBase::setDeviceManager(ICamDeviceManager* manager) {
  mpDeviceManager = manager;
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
Cam3DeviceBase::i_closeDevice() {
  MY_LOGD("");
  i_uninitialize();
  return mpDeviceManager->close();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
Cam3DeviceBase::i_initialize(camera3_callback_ops const* callback_ops) {
  std::lock_guard<std::mutex> _DevOpsLock(mDevOpsLock);
  //
  mpCallbackOps = callback_ops;
  //
  return onInitializeLocked();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
Cam3DeviceBase::i_uninitialize() {
  std::lock_guard<std::mutex> _DevOpsLock(mDevOpsLock);
  return onUninitializeLocked();
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
Cam3DeviceBase::i_configure_streams(
    camera3_stream_configuration_t* stream_list) {
  std::lock_guard<std::mutex> _DevOpsLock(mDevOpsLock);
  return onConfigureStreamsLocked(stream_list);
}

/******************************************************************************
 *
 ******************************************************************************/
MERROR
Cam3DeviceBase::i_flush() {
  std::lock_guard<std::mutex> _DevOpsLock(mDevOpsLock);
  return onFlushLocked();
}

/******************************************************************************
 *
 ******************************************************************************/
void Cam3DeviceBase::i_dump(int /*fd*/) {
  MY_LOGW("[TODO] Cam3DeviceBase::i_dump");
}
