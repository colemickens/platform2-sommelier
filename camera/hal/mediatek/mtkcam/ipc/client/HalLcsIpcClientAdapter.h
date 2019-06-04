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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_HALLCSIPCCLIENTADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_HALLCSIPCCLIENTADAPTER_H_

#include <vector>

#include "mtkcam/aaa/lcs/lcs_hal.h"
#include "Hal3aIpcCommon.h"
#include "Mediatek3AClient.h"

namespace IPCLCS {

extern "C" LcsHal* createInstance_HalLcs_Client(const char* user_name,
                                                const MUINT32& sensor_idx);

class HalLcsIpcClientAdapter : public LcsHal {
 public:
  friend LcsHal* createInstance_HalLcs_Client(const char* user_name,
                                              const MUINT32& sensor_idx);
  HalLcsIpcClientAdapter(const HalLcsIpcClientAdapter&) = delete;
  HalLcsIpcClientAdapter& operator=(const HalLcsIpcClientAdapter&) = delete;
  virtual ~HalLcsIpcClientAdapter();

  MVOID DestroyInstance(char const* userName) override;
  MINT32 Init() override;
  MINT32 Uninit() override;
  MINT32 ConfigLcsHal(const LCS_HAL_CONFIG_DATA& r_config_data) override;

 protected:
  explicit HalLcsIpcClientAdapter(const MINT32 sensor_idx);
  MBOOL sendRequest(IPC_CMD cmd,
                    ShmMemInfo* meminfo,
                    int group = IPC_GROUP_LCS);

 private:
  bool m_initialized;
  int m_sensor_idx;
  Mtk3aCommon m_ipc_common;
  ShmMemInfo m_meminfo_create;
  ShmMemInfo m_meminfo_init;
  ShmMemInfo m_meminfo_config;
  ShmMemInfo m_meminfo_uninit;
  std::vector<ShmMem> mv_mems;
};

}  // namespace IPCLCS

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_HALLCSIPCCLIENTADAPTER_H_
