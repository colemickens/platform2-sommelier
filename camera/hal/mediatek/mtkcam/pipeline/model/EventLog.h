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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_EVENTLOG_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_EVENTLOG_H_
//
#include <mtkcam/utils/std/LogTool.h>
#include <mtkcam/utils/std/RingBuffer.h>
//
#include <mutex>
#include <string>
#include <utility>
//

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *
 ******************************************************************************/
/**
 *
 *  +---------+---------+---------+----------++----------+
 *  | The first buffer (to keep the oldest logs)         |
 *  |                                                    |
 *  +---------+---------+---------+----------++----------+
 *
 *  +---------+---------+---------+----------++----------+
 *  | The second buffer (to keep the latest logs)        |
 *  |                                                    |
 *  +---------+---------+---------+----------++----------+
 *
 * The first buffer: a linear buffer to keep the oldest logs
 *  - When it is full, new logs will start to be written to the second buffer.
 *
 * The second buffer: a ring buffer to keep the latest logs
 *  - When it is full, the oldest logs in this buffer will be overwritten with
 *    the latest logs.
 *
 */
class EventLog {
 protected:  ////    Definitions.
  struct Item {
    struct timespec timestamp;
    std::string event;
  };

  typedef NSCam::Utils::RingBuffer<Item> LogBufferT;

 protected:  ////    Data Members.
  static const size_t kDefaultOldestBufferCapacity = 0;
  static const size_t kDefaultLatestBufferCapacity = 25;

  mutable std::mutex mLogLock;
  LogBufferT mLogBuffer1;
  LogBufferT mLogBuffer2;
  NSCam::Utils::LogTool* mLogTool = nullptr;

 public:  ////    Operations.
  EventLog(const size_t nLatestBufferCapacity = kDefaultLatestBufferCapacity,
           const size_t nOldestBufferCapacity = kDefaultOldestBufferCapacity)
      : mLogBuffer1(nOldestBufferCapacity),
        mLogBuffer2(nLatestBufferCapacity),
        mLogTool(NSCam::Utils::LogTool::get()) {}

  template <class T>
  auto add(const T& event) {
    Item item;
    item.event = event;
    mLogTool->getCurrentLogTime(&item.timestamp);

    std::lock_guard<std::mutex> _l(mLogLock);

    if (mLogBuffer1.size() < mLogBuffer1.capacity()) {
      mLogBuffer1.push_back(std::move(item));
    } else {
      mLogBuffer2.push_back(std::move(item));
    }
  }
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_EVENTLOG_H_
