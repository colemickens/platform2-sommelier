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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_COMMON_POLLERTHREAD_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_COMMON_POLLERTHREAD_H_

#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "V4L2StreamNode.h"

#include <mtkcam/utils/imgbuf/IImageBuffer.h>
#include <mtkcam/drv/def/IPostProcDef.h>

#include <Errors.h>

#include <cros-camera/v4l2_device.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v4l2 {

#define EVENT_POLL_TIMEOUT 1000  // 1000 milliseconds timeout

enum PollerCmd { CMD_INVALID = -1, CMD_INIT, CMD_POLL, CMD_FLUSH };

struct Poller {
  PollerCmd cmd;
  uint8_t meta[64];
  explicit Poller(PollerCmd c) : cmd(c) {}
};

/**
 * \class IPollEventListener
 *
 * Abstract interface implemented by entities interested on receiving
 * notifications from IPU PollerThread.
 *
 * Notifications are sent whenever the poll returns.
 */
class IPollEventListener {
 public:
  enum PollEventMessageId {
    POLL_EVENT_ID_EVENT = 0,
    POLL_EVENT_ID_TIMEOUT,
    POLL_EVENT_ID_ERROR,
  };

  struct PollEventMessageData {
    const std::vector<std::shared_ptr<V4L2Device>>* activeDevices;
    const std::vector<std::shared_ptr<V4L2Device>>* inactiveDevices;
    std::vector<std::shared_ptr<V4L2Device>>*
        polledDevices;  // NOTE: notified entity is allowed to change this!
    const std::vector<std::shared_ptr<V4L2Device>>*
        requestedDevices;  // Requested active devices
    int reqId;
    int pollStatus;
    unsigned int timeoutMs;
  };

  struct PollEventMessage {
    PollEventMessageId id;
    PollEventMessageData data;
  };

  virtual status_t notifyPollEvent(PollEventMessage* msg) = 0;
  virtual ~IPollEventListener() {}
};  // IPollEventListener

class VISIBILITY_PUBLIC PollerThread {
 public:
  PollerThread();
  virtual ~PollerThread();

  // Public Methods
  virtual status_t init(const std::vector<std::shared_ptr<V4L2Device>>& devices,
                        IPollEventListener* observer,
                        int events = POLLPRI | POLLIN | POLLERR);
  virtual status_t queueRequest(
      int reqId = 0,
      int timeout = EVENT_POLL_TIMEOUT,
      std::vector<std::shared_ptr<V4L2Device>>* devices = nullptr);
  virtual status_t flush(bool sync = false, bool clear = false);

  // Private Members
 private:
  struct MessageInit {
    IPollEventListener* observer;
    int events;
    std::vector<std::shared_ptr<V4L2Device>> devices;
  };

  struct MessageFlush {
    bool clearVectors;
  };

  struct MessagePollRequest {
    unsigned int reqId;
    unsigned int timeout;
    std::vector<std::shared_ptr<V4L2Device>> devices;
  };

  std::vector<std::shared_ptr<V4L2Device>> mPollingDevices;
  std::vector<std::shared_ptr<V4L2Device>> mActiveDevices;
  std::vector<std::shared_ptr<V4L2Device>> mInactiveDevices;
  std::vector<std::shared_ptr<V4L2Device>>
      mRequestedDevices;  // Requested active devices

  int mFlushFd[2];
  int mEvents;
  std::thread mPollerThread;
  IPollEventListener* mListener;  // one listener per PollerThread, PollerThread
                                  // doesn't has ownership
  mutable std::mutex mLock;
  std::condition_variable mCondition;
  std::deque<Poller> mQueue;

  // Private Methods
 private:
  virtual status_t handleInit(MessageInit* msg);
  virtual status_t handlePollRequest(MessagePollRequest* msg);
  virtual status_t handleFlush(MessageFlush* msg);
  virtual status_t notifyListener(IPollEventListener::PollEventMessage* msg);
  static status_t threadLoop(void* user);
};

};      // namespace v4l2
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_DRV_IOPIPE_SRC_COMMON_POLLERTHREAD_H_
