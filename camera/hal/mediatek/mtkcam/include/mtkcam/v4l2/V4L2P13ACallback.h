/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2P13ACALLBACK_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2P13ACALLBACK_H_

// MTKCAM
#include <mtkcam/aaa/IHal3A.h>
#include <mtkcam/aaa/IHal3ACb.h>
// V4L2
#include <mtkcam/v4l2/ipc_queue.h>
#include <mtkcam/v4l2/V4L2DriverWorker.h>
//
#include <cstdint>
#include <memory>

namespace v4l2 {

using NS3Av3::E3ACtrl_IPC_P1_NotifyCb;
using NS3Av3::E3ACtrl_IPC_P1_NotifyCbEnable;
using NS3Av3::IHal3A;
using NS3Av3::IHal3ACb;
using NS3Av3::IPC_P1NotifyCb_Proc_Finish_T;
using NS3Av3::IPC_P1NotifyCb_T;

class VISIBILITY_PUBLIC V4L2P13ACallback
    : public V4L2DriverWorker,
      public IPCQueueClient<IPC_P1NotifyCb_T> {
 public:
  V4L2P13ACallback(int sensorIdx, IHal3ACb* notifier);
  ~V4L2P13ACallback();

 public:
  void ack();
  void validate();
  void invalidate();

  // re-implementations of V4L2DriverWorker
 protected:
  void job() override;

 public:
  int start() override;
  int stop() override;

  // re-implementations of IPCQueueClient
 protected:
  int _ipc_acquire_param(IPC_P1NotifyCb_T* pResult,
                         uint32_t timeoutMs) override;

 private:
  IHal3ACb* m_pNotifier;
  int m_logLevel;
  std::shared_ptr<IHal3A> m_pHal3A;
};

};      // namespace v4l2
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2P13ACALLBACK_H_
