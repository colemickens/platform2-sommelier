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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2HWEVENTMGR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2HWEVENTMGR_H_

// MTKCAM
#include <mtkcam/aaa/IHal3A.h>

#ifdef __ANDROID__
#include <mtkcam/drv/iopipe/CamIO/INormalPipe.h>
#else
#include <mtkcam/drv/iopipe/CamIO/V4L2IIOPipe.h>
#endif

// MTKCAM/V4L2
#include <mtkcam/v4l2/ipc_queue.h>
#include <mtkcam/v4l2/V4L2DriverWorker.h>

#include <memory>
#include <string>
#include <mtkcam/def/common.h>

namespace v4l2 {

using NS3Av3::E3ACtrl_IPC_P1_HwSignal;
using NS3Av3::IHal3A;
using NSCam::NSIoPipe::NSCamIOPipe::EPipeSignal;
using NSCam::NSIoPipe::NSCamIOPipe::V4L2IEventPipe;

class VISIBILITY_PUBLIC V4L2HwEventWorker : public V4L2DriverWorker {
 public:
  V4L2HwEventWorker(uint32_t sensorIdx,
                    EPipeSignal signalToListen,
                    const char* szCallerName);

  virtual ~V4L2HwEventWorker();

  // re-implementations of V4L2DriverWorker
 public:
  int start() override;
  int stop() override;

 public:
  void signal();  // manually send signal to terminate INormalPipe

  // re-implementations of V4L2DriverWorker
 protected:
  void job() override;

 private:
  int m_sensorIdx;
  int m_logLevel;
  EPipeSignal m_listenedSignal;

  // caller name must be unique between every instances, or INormalPipe may
  // have unexpected behaviors.
  std::string m_eventName;
  std::shared_ptr<V4L2IEventPipe> m_pEventPipe;
  std::shared_ptr<IHal3A> m_pHal3A;
};

};  // namespace v4l2

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_V4L2HWEVENTMGR_H_
