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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_NR3DIPCCLIENTADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_NR3DIPCCLIENTADAPTER_H_

#include <libeis/MTKEis.h>
#include <vector>

#include "Hal3aIpcCommon.h"
#include "Mediatek3AClient.h"

using NSCam::MRESULT;

namespace IPC3DNR {
extern "C" MTKEis* createInstance_3DNR_Client(const MINT32 sensor_idx,
                                              const char* user_name);

class NR3DIpcClientAdapter : public MTKEis {
 public:
  friend MTKEis* createInstance_3DNR_Client(const MINT32 sensor_idx,
                                            const char* user_name);
  virtual ~NR3DIpcClientAdapter();
  void destroyInstance() override;
  MRESULT EisInit(void* initInData) override;
  MRESULT EisMain(EIS_RESULT_INFO_STRUCT* eisResult) override;
  MRESULT EisReset() override;
  MRESULT EisFeatureCtrl(MUINT32 FeatureID,
                         void* pParaIn,
                         void* pParaOut) override;

 protected:
  MINT32 sendRequest(IPC_CMD cmd,
                     ShmMemInfo* meminfo,
                     int group = IPC_GROUP_3DNR);
  explicit NR3DIpcClientAdapter(const MINT32 sensor_idx);

 public:
  bool m_initialized;
  int m_sensor_idx;
  Mtk3aCommon m_ipc_common;
  ShmMemInfo mMemCreate;
  ShmMemInfo mMemDestroy;
  ShmMemInfo mMemReset;
  ShmMemInfo mMemInit;
  ShmMemInfo mMemMain;
  ShmMemInfo mMemFeatureCtrl;
  std::vector<ShmMem> mv_mems;
};
}  // namespace IPC3DNR
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_NR3DIPCCLIENTADAPTER_H_
