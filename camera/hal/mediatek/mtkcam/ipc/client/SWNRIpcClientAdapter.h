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
#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_SWNRIPCCLIENTADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_SWNRIPCCLIENTADAPTER_H_

#include <unordered_map>
#include <vector>

#include "mtkcam/aaa/ICaptureNR.h"
#include "Hal3aIpcCommon.h"
#include "Mediatek3AClient.h"

namespace IPCSWNR {

extern "C" ISwNR* createInstance_SWNR_Client(const MINT32 sensor_idx);

class SWNRIpcClientAdapter : public ISwNR {
 public:
  friend ISwNR* createInstance_SWNR_Client(const MINT32 sensor_idx);
  SWNRIpcClientAdapter(const SWNRIpcClientAdapter&) = delete;
  SWNRIpcClientAdapter& operator=(const SWNRIpcClientAdapter&) = delete;
  virtual ~SWNRIpcClientAdapter();

  MBOOL doSwNR(const SWNRParam& swnr_param,
               NSCam::IImageBuffer* p_buf) override;
  MBOOL getDebugInfo(NSCam::IMetadata* hal_metadata) override;

 protected:
  explicit SWNRIpcClientAdapter(const MINT32 sensor_idx);
  MBOOL sendRequest(IPC_CMD cmd,
                    ShmMemInfo* meminfo,
                    int group = IPC_GROUP_SWNR);

 private:
  bool m_initialized;
  int m_sensor_idx;
  Mtk3aCommon m_ipc_common;
  ShmMemInfo m_meminfo_create;
  ShmMemInfo m_meminfo_destroy;
  ShmMemInfo m_meminfo_do_swnr;
  ShmMemInfo m_meminfo_get_debuginfo;
  std::vector<ShmMem> mv_mems;
  std::unordered_map<int, int>
      m_map_swnr_buf;  // key: buffer fd, value: buffer handle
};

}  // namespace IPCSWNR
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_CLIENT_SWNRIPCCLIENTADAPTER_H_
