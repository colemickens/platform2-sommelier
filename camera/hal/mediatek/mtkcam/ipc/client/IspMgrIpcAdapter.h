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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_ISPMGRIPCADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_ISPMGRIPCADAPTER_H_

#include "mtkcam/aaa/IIspMgr.h"
#include "Mediatek3AClient.h"
#include "Hal3aIpcCommon.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace NS3Av3 {

VISIBILITY_PUBLIC extern "C" IIspMgrIPC* getInstance_ispMgr_IPC(
    const char* strUser);

class IspMgrIpcAdapter : public IIspMgrIPC {
 public:
  friend IIspMgrIPC* getInstance_ispMgr_IPC(const char* strUser);
  MVOID queryLCSOParams(LCSO_Param* param);
  MVOID postProcessNR3D(MINT32 sensorIndex,
                        NR3D_Config_Param* param,
                        void* pTuning);
  void uninit(const char* strUser);
  virtual ~IspMgrIpcAdapter();

 protected:
  IspMgrIpcAdapter();
  MINT32 sendRequest(IPC_CMD cmd,
                     ShmMemInfo* memInfo,
                     int32_t group = IPC_GROUP_ISPMGR);

 private:
  void init(const char* strUser);

  Mtk3aCommon mCommon;
  bool mInitialized;

  ShmMemInfo imMemCreate;
  ShmMemInfo imMemQueryLCSO;
  ShmMemInfo imMemPPNR3D;
  std::vector<ShmMem> imMem;

  std::mutex mInitMutex;
  std::unordered_map<std::string, MINT32> m_Users;
  std::unordered_map<int32_t, int32_t> m_map_p2tuningbuf_handles;
};

}  // namespace NS3Av3

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_ISPMGRIPCADAPTER_H_
