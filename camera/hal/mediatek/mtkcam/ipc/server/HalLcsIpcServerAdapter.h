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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_HALLCSIPCSERVERADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_HALLCSIPCSERVERADAPTER_H_

#include <algorithm>
#include <memory>

#include "mtkcam/aaa/lcs/lcs_hal.h"
#include "IPCCommon.h"

namespace IPCLCS {

class HalLcsIpcServerAdapter {
 public:
  HalLcsIpcServerAdapter(const HalLcsIpcServerAdapter&) = delete;
  HalLcsIpcServerAdapter& operator=(const HalLcsIpcServerAdapter&) = delete;
  HalLcsIpcServerAdapter() {
    std::fill_n(msp_lcs, IPC_MAX_SENSOR_NUM, nullptr);
  }
  virtual ~HalLcsIpcServerAdapter() {}

  int extractSensor(void* addr);
  int create(void* addr, int dataSize);
  int init(void* addr, int dataSize);
  int config(void* addr, int dataSize);
  int uninit(void* addr, int dataSize);

 private:
  int extractSensorId(void* addr) const;

  std::mutex create_lock;
  LcsHal* msp_lcs[IPC_MAX_SENSOR_NUM];
};

}  // namespace IPCLCS
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_HALLCSIPCSERVERADAPTER_H_
