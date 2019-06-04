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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_SWNRIPCSERVERADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_SWNRIPCSERVERADAPTER_H_

#include <algorithm>

#include "mtkcam/aaa/ICaptureNR.h"
#include "IPCCommon.h"

namespace IPCSWNR {

class SWNRIpcServerAdapter {
 public:
  SWNRIpcServerAdapter(const SWNRIpcServerAdapter&) = delete;
  SWNRIpcServerAdapter& operator=(const SWNRIpcServerAdapter&) = delete;
  SWNRIpcServerAdapter() : msp_swnr{} {
    std::fill_n(msp_swnr, IPC_MAX_SENSOR_NUM, nullptr);
  }
  virtual ~SWNRIpcServerAdapter() = default;

  int extractSensor(void* addr);
  int create(void* addr, int dataSize);
  int destroy(void* addr, int dataSize);
  int doSwNR(void* addr, int dataSize);
  int getDebugInfo(void* addr, int dataSize);

 private:
  int extractSensorId(void* addr) const;

  std::mutex create_lock;
  ISwNR* msp_swnr[IPC_MAX_SENSOR_NUM];
};

}  // namespace IPCSWNR
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_SWNRIPCSERVERADAPTER_H_
