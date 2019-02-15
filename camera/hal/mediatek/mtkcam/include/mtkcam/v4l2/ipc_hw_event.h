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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_IPC_HW_EVENT_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_IPC_HW_EVENT_H_

// STL
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace v4l2 {
struct P1Event {
  int32_t event;  // refer to EPipeSignal
  int16_t sensorIdx;
  int16_t sensorDev;
  int32_t requestNo;
  //
  P1Event() : event(0), sensorIdx(0), sensorDev(0), requestNo(0) {}
};

};      // namespace v4l2
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_V4L2_IPC_HW_EVENT_H_
