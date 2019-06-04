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

#include <camera/hal/mediatek/mtkcam/ipc/client/HalLcsIpcClientAdapter.h>

#include "mtkcam/utils/std/Log.h"
#include "Hal3aIpcCommon.h"
#include "IPCLCS.h"

#undef LOG_TAG
#define LOG_TAG "LCS_IPC_CLIENT"

namespace IPCLCS {

VISIBILITY_PUBLIC
extern "C" LcsHal* createInstance_HalLcs_Client(const char* user_name,
                                                const MUINT32& sensor_idx) {
  MY_LOGD("created by user:%s", user_name);

  if (CC_UNLIKELY(sensor_idx >= IPC_MAX_SENSOR_NUM || sensor_idx < 0)) {
    MY_LOGE("sensor index %d is illegal, should be 0~%d", sensor_idx,
            IPC_MAX_SENSOR_NUM);
    return nullptr;
  }

  HalLcsIpcClientAdapter* lcs_adapter = new HalLcsIpcClientAdapter(sensor_idx);

  if (CC_UNLIKELY(!lcs_adapter->m_initialized)) {
    delete lcs_adapter;
    lcs_adapter = nullptr;
  }

  return static_cast<LcsHal*>(lcs_adapter);
}

HalLcsIpcClientAdapter::HalLcsIpcClientAdapter(const MINT32 sensor_idx)
    : m_initialized(false), m_sensor_idx(sensor_idx) {
  mv_mems = {
      {"/mtkLCS_Create", sizeof(CreateParams), &m_meminfo_create, false},
      {"/mtkLCS_Init", sizeof(InitParams), &m_meminfo_init, false},
      {"/mtkLCS_Config", sizeof(ConfigParams), &m_meminfo_config, false},
      {"/mtkLCS_Uninit", sizeof(UninitParams), &m_meminfo_uninit, false}};

  TRACE_FUNC_ENTER();

  int ret = false;
  m_ipc_common.init(m_sensor_idx);
  ret = m_ipc_common.allocateAllShmMems(&mv_mems);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("construction fail while allocate ipc shared memroy");
    return;
  }

  ret = sendRequest(IPC_LCS_CREATE, &m_meminfo_create);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("construction fail while create");
    return;
  }

  m_initialized = true;

  TRACE_FUNC_EXIT();
}

HalLcsIpcClientAdapter::~HalLcsIpcClientAdapter() {}

MVOID HalLcsIpcClientAdapter::DestroyInstance(char const* userName) {
  m_ipc_common.releaseAllShmMems(&mv_mems);
  delete this;
}

MINT32 HalLcsIpcClientAdapter::Init() {
  TRACE_FUNC_ENTER();
  int ret = sendRequest(IPC_LCS_INIT, &m_meminfo_init);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("sync request fail with error status %d", ret);
    return LCS_RETURN_API_FAIL;
  }

  TRACE_FUNC_EXIT();
  return OK;
}

MINT32 HalLcsIpcClientAdapter::Uninit() {
  TRACE_FUNC_ENTER();
  int ret = sendRequest(IPC_LCS_UNINIT, &m_meminfo_uninit);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("sync request fail with error status %d", ret);
    return LCS_RETURN_API_FAIL;
  }

  TRACE_FUNC_EXIT();
  return OK;
}

MINT32 HalLcsIpcClientAdapter::ConfigLcsHal(
    const LCS_HAL_CONFIG_DATA& r_config_data) {
  TRACE_FUNC_ENTER();
  ConfigParams* params = static_cast<ConfigParams*>(m_meminfo_config.mAddr);

  params->config_data = r_config_data;
  int ret = sendRequest(IPC_LCS_CONFIG, &m_meminfo_config);
  if (CC_UNLIKELY(!ret)) {
    MY_LOGE("sync request fail with error status %d", ret);
    return LCS_RETURN_API_FAIL;
  }

  TRACE_FUNC_EXIT();
  return OK;
}

MBOOL HalLcsIpcClientAdapter::sendRequest(IPC_CMD cmd,
                                          ShmMemInfo* meminfo,
                                          int group) {
  CommonParams* params = static_cast<CommonParams*>(meminfo->mAddr);
  params->sensor_idx = m_sensor_idx;

  return m_ipc_common.requestSync(cmd, meminfo->mHandle, group);
}

}  // namespace IPCLCS
