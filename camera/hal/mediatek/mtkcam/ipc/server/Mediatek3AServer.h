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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_MEDIATEK3ASERVER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_MEDIATEK3ASERVER_H_

#include <memory>
#include <sys/un.h>

#include <base/bind.h>
#include <base/threading/thread.h>
#include <unordered_map>

#include <Errors.h>
#include <IHal3A.h>
#include <IHal3ACb.h>

#include "cros-camera/camera_algorithm.h"
#include "IPCCommon.h"
#include "Hal3aIpcServerAdapter.h"
#include "SWNRIpcServerAdapter.h"
#include "HalLcsIpcServerAdapter.h"
#include "IspMgrIpcServerAdapter.h"
#include "NR3DIpcServerAdapter.h"
#include "FDIpcServerAdapter.h"

namespace NS3Av3 {

class Mediatek3AServer {
 public:
  static void init();
  static void deInit();

  static Mediatek3AServer* getInstance() { return mInstance; }

  int32_t initialize(const camera_algorithm_callback_ops_t* callback_ops);
  int32_t registerBuffer(int buffer_fd);
  void request(uint32_t req_id,
               const uint8_t req_header[],
               uint32_t size,
               int32_t buffer_handle);
  void deregisterBuffers(const int32_t buffer_handles[], uint32_t size);

  void notify(uint32_t req_id, uint32_t rc);

 private:
  Mediatek3AServer();
  ~Mediatek3AServer();

  int parseReqHeader(const uint8_t req_header[], uint32_t size);

  struct MsgReq {
    uint32_t req_id;
    int32_t buffer_handle;
  };
  void returnCallback(uint32_t req_id, status_t status, int32_t buffer_handle);
  void handleRequest(MsgReq msg);

 private:
  static Mediatek3AServer* mInstance;
  NS3Av3::Hal3aIpcServerAdapter adapter_3a;
  IPCSWNR::SWNRIpcServerAdapter adapter_swnr;
  IPCLCS::HalLcsIpcServerAdapter adapter_lcs;
  NS3Av3::IspMgrIpcServerAdapter adapter_ispmgr;
  IPC3DNR::NR3DIpcServerAdapter adapter_nr3d;
  FDIpcServerAdapter adapter_fd;

  std::unique_ptr<base::Thread> mThreads[IPC_GROUP_NUM * IPC_MAX_SENSOR_NUM];
  const camera_algorithm_callback_ops_t* mCallback;

  // key: shared memory fd from client
  // value: handle that returns from RegisterBuffer()
  std::unordered_map<int32_t, int32_t> mHandles;

  typedef struct {
    int32_t fd;
    void* addr;
    size_t size;
  } ShmInfo;
  // key: handle that returns from RegisterBuffer()
  // value: shared memory fd and mapped address
  std::unordered_map<int32_t, ShmInfo> mShmInfoMap;

  int32_t mHandleSeed;
  mutable pthread_rwlock_t mRWLock;
};

}  // namespace NS3Av3

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_IPC_SERVER_MEDIATEK3ASERVER_H_
