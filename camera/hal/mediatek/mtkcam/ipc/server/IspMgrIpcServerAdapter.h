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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_ISPMGRIPCSERVERADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_ISPMGRIPCSERVERADAPTER_H_

#include "mtkcam/aaa/IIspMgr.h"

namespace NS3Av3 {

class IspMgrIpcServerAdapter {
 public:
  IspMgrIpcServerAdapter() { msp_ispmgr = NULL; }
  virtual ~IspMgrIpcServerAdapter() {}

  int create(void* addr, int dataSize);
  int ppnr3d(void* addr, int dataSize);
  int querylcso(void* addr, int dataSize);

 private:
  std::mutex create_lock;
  NS3Av3::IIspMgr* msp_ispmgr;
};

}  // namespace NS3Av3

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_ISPMGRIPCSERVERADAPTER_H_
