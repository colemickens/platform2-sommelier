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

#include <cutils/compiler.h>
#include <stdio.h>

#include <camera/hal/mediatek/mtkcam/ipc/server/HalLcsIpcServerAdapter.h>

#include "mtkcam/utils/std/Log.h"
#include "IPCLCS.h"
#include "Errors.h"

#undef LOG_TAG
#define LOG_TAG "LCS_IPC_SERVER"

namespace IPCLCS {

int HalLcsIpcServerAdapter::extractSensor(void* addr) {
  int s_idx;
  CommonParams* params = static_cast<CommonParams*>(addr);
  s_idx = params->sensor_idx;
  if (CC_UNLIKELY(s_idx >= IPC_MAX_SENSOR_NUM || s_idx < 0)) {
    MY_LOGE("sensor index %d is illegal, should less than %d and is postive",
            s_idx, IPC_MAX_SENSOR_NUM);
    return -1;
  }
  return s_idx;
}

int HalLcsIpcServerAdapter::create(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  CheckError((dataSize < sizeof(CreateParams)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  int sensor_idx = 0;
  sensor_idx = extractSensor(addr);
  if (sensor_idx < 0)
    return -1;

  std::lock_guard<std::mutex> _lk(create_lock);
  if (!msp_lcs[sensor_idx]) {
    MY_LOGD("MAKE_LCS, sensor index is %d", sensor_idx);
    msp_lcs[sensor_idx] = (MAKE_LcsHal(LOG_TAG, sensor_idx));
  }

  return OK;
}

int HalLcsIpcServerAdapter::init(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  CheckError((dataSize < sizeof(InitParams)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  int sensor_idx = 0;
  sensor_idx = extractSensor(addr);
  if (sensor_idx < 0)
    return -1;

  if (CC_UNLIKELY(!msp_lcs[sensor_idx])) {
    MY_LOGE(
        "swnr pointer at sensor index %d is null, should be created firstly",
        sensor_idx);
    return INVALID_OPERATION;
  }

  TRACE_FUNC_EXIT();
  return msp_lcs[sensor_idx]->Init();
}

int HalLcsIpcServerAdapter::config(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  CheckError((dataSize < sizeof(ConfigParams)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  int sensor_idx = 0;
  sensor_idx = extractSensor(addr);
  if (sensor_idx < 0)
    return -1;

  if (CC_UNLIKELY(!msp_lcs[sensor_idx])) {
    MY_LOGE(
        "swnr pointer at sensor index %d is null, should be created firstly",
        sensor_idx);
    return INVALID_OPERATION;
  }

  ConfigParams* params = static_cast<ConfigParams*>(addr);
  if (!params) {
    MY_LOGE("LCS Config Buffer is NULL");
    return -1;
  }

  TRACE_FUNC_EXIT();
  return msp_lcs[sensor_idx]->ConfigLcsHal(params->config_data);
}

int HalLcsIpcServerAdapter::uninit(void* addr, int dataSize) {
  TRACE_FUNC_ENTER();
  CheckError((dataSize < sizeof(UninitParams)), UNKNOWN_ERROR,
             "@%s, buffer is small", __FUNCTION__);
  int sensor_idx = 0;
  sensor_idx = extractSensor(addr);
  if (sensor_idx < 0)
    return -1;

  if (CC_UNLIKELY(!msp_lcs[sensor_idx])) {
    MY_LOGE(
        "swnr pointer at sensor index %d is null, should be created firstly",
        sensor_idx);
    return INVALID_OPERATION;
  }

  int ret = msp_lcs[sensor_idx]->Uninit();

  TRACE_FUNC_EXIT();
  return ret;
}

}  // namespace IPCLCS
