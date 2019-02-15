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

#define LOG_TAG "PollerThread"

#include <fcntl.h>
#include <unistd.h>

#include <mtkcam/utils/std/Log.h>
#include "mtkcam/drv/iopipe/src/common/PollerThread.h"

using NSCam::status_t;
using NSCam::v4l2::PollerThread;

namespace NSCam {
namespace v4l2 {

/******************************************************************************
 *
 ******************************************************************************/
PollerThread::PollerThread() : mEvents(POLLPRI | POLLIN | POLLERR) {
  LOGD("+");

  mFlushFd[0] = -1;
  mFlushFd[1] = -1;

  /* make sure all members had been initialized, then start the thread */
  mPollerThread = std::thread(threadLoop, this);
}

PollerThread::~PollerThread() {
  LOGD("+");

  /* join thread if necessary */
  if (mPollerThread.joinable()) {
    mPollerThread.join();
  }

  close(mFlushFd[0]);
  close(mFlushFd[1]);
  mFlushFd[0] = -1;
  mFlushFd[1] = -1;
}

status_t PollerThread::init(
    const std::vector<std::shared_ptr<V4L2Device>>& devices,
    IPollEventListener* observer,
    int events) {
  LOGD("+");
  Poller poller(CMD_INIT);
  MessageInit* msg = new (poller.meta) MessageInit();

  msg->devices = devices;  // copy the vector
  msg->observer = observer;
  msg->events = events;

  {
    std::lock_guard<std::mutex> _l(mLock);
    mQueue.push_back(std::move(poller));
    mCondition.notify_one();
  }

  return NO_ERROR;
}

status_t PollerThread::handleInit(MessageInit* msg) {
  LOGD("+");
  status_t status = NO_ERROR;

  if (mFlushFd[1] != -1 || mFlushFd[0] != -1) {
    close(mFlushFd[0]);
    close(mFlushFd[1]);
    mFlushFd[0] = -1;
    mFlushFd[1] = -1;
  }

  status = pipe(mFlushFd);
  if (status < 0) {
    LOGE("Failed to create Flush pipe: %s", strerror(errno));
    return NO_INIT;
  }

  /**
   * make the reading end of the pipe non blocking.
   * This helps during flush to read any information left there without
   * blocking
   */
  status = fcntl(mFlushFd[0], F_SETFL, O_NONBLOCK);
  if (status < 0) {
    LOGE("Fail to set flush pipe flag: %s", strerror(errno));
    return NO_INIT;
  }

  if (msg->devices.size() == 0) {
    LOGE("%s, No devices provided", __FUNCTION__);
    return BAD_VALUE;
  }

  if (msg->observer == nullptr) {
    LOGE("%s, No observer provided", __FUNCTION__);
    return BAD_VALUE;
  }

  mPollingDevices = msg->devices;
  mEvents = msg->events;

  // attach listener.
  mListener = msg->observer;

  msg->~MessageInit();

  return status;
}

status_t PollerThread::queueRequest(
    int reqId, int timeout, std::vector<std::shared_ptr<V4L2Device>>* devices) {
  Poller poller(CMD_POLL);
  MessagePollRequest* msg = new (poller.meta) MessagePollRequest();

  msg->reqId = reqId;
  msg->timeout = timeout;

  if (devices) {
    msg->devices = *devices;
  }

  {
    std::lock_guard<std::mutex> _l(mLock);
    mQueue.push_back(std::move(poller));
    mCondition.notify_one();
  }

  return NO_ERROR;
}

status_t PollerThread::handlePollRequest(MessagePollRequest* msg) {
  status_t status = NO_ERROR;
  int ret;
  IPollEventListener::PollEventMessage outMsg;

  if (msg->devices.size() > 0) {
    mPollingDevices = msg->devices;
  }
  mRequestedDevices.clear();

  do {
    std::vector<V4L2Device*> polling_devices;
    for (const auto& it : mPollingDevices) {
      polling_devices.push_back(it.get());
    }
    std::vector<V4L2Device*> active_devices;
    ret = V4L2DevicePoller(polling_devices, mFlushFd[0])
              .Poll(msg->timeout, mEvents, &active_devices);
    if (ret < 0) {
      outMsg.id = IPollEventListener::POLL_EVENT_ID_ERROR;
    } else if (ret == 0) {
      outMsg.id = IPollEventListener::POLL_EVENT_ID_TIMEOUT;
    } else {
      outMsg.id = IPollEventListener::POLL_EVENT_ID_EVENT;
    }
    outMsg.data.reqId = msg->reqId;
    mActiveDevices.clear();
    mInactiveDevices.clear();
    for (const auto& it : mPollingDevices) {
      if (std::find(active_devices.begin(), active_devices.end(), it.get()) !=
          active_devices.end()) {
        mActiveDevices.push_back(it);
        mRequestedDevices.push_back(it);
      } else {
        mInactiveDevices.push_back(it);
      }
    }
    outMsg.data.activeDevices = &mActiveDevices;
    outMsg.data.inactiveDevices = &mInactiveDevices;
    outMsg.data.polledDevices = &mPollingDevices;
    outMsg.data.requestedDevices = &mRequestedDevices;
    outMsg.data.pollStatus = ret;
    outMsg.data.timeoutMs = msg->timeout;
    status = notifyListener(&outMsg);
  } while (status == -EAGAIN);

  msg->~MessagePollRequest();

  return status;
}

/**
 * flush()
 * this method is done to interrupt the polling.
 * We first empty the Q for any polling request and then
 * a value is written to a polled fd, which will make the poll returning
 *
 * There are 2 variants an asyncrhonous one that will not wait for the thread
 * to complete the current request and the synchronous one that will send
 * a message to the Q
 *
 * This can be called on an uninitialized Poller also, but the flush will then
 * only empty the message queue and the vectors.
 *
 */
status_t PollerThread::flush(bool sync, bool clear) {
  Poller poller(CMD_FLUSH);
  MessageFlush* msg = new (poller.meta) MessageFlush();

  msg->clearVectors = clear;

  if (mFlushFd[1] != -1) {
    char buf = 0xf;  // random value to write to flush fd.
    unsigned int size = write(mFlushFd[1], &buf, sizeof(char));
    if (size != sizeof(char)) {
      LOGW("Flush write not completed");
    }
  }

  {
    std::lock_guard<std::mutex> _l(mLock);
    mQueue.push_back(std::move(poller));
    mCondition.notify_one();
  }

  if (sync) {
    mPollerThread.join();
  }

  return NO_ERROR;
}

status_t PollerThread::handleFlush(MessageFlush* msg) {
  /**
   * read the pipe just in case there was nothing to flush.
   * this ensures that the pipe is empty for the next try.
   * this is safe because the reading end is non blocking.
   */
  if (msg->clearVectors) {
    mPollingDevices.clear();
    mActiveDevices.clear();
    mInactiveDevices.clear();
    mRequestedDevices.clear();
  }

  char readbuf;
  if (mFlushFd[0] != -1) {
    unsigned int size =
        read(mFlushFd[0], reinterpret_cast<void*>(&readbuf), sizeof(char));
    if (size != sizeof(char)) {
      LOGW("Flush read not completed.");
    }
  }

  msg->~MessageFlush();
  return NO_ERROR;
}

/** Listener Methods **/
status_t PollerThread::notifyListener(
    IPollEventListener::PollEventMessage* msg) {
  status_t status = OK;

  if (mListener == nullptr) {
    return BAD_VALUE;
  }

  status =
      mListener->notifyPollEvent((IPollEventListener::PollEventMessage*)msg);

  return status;
}

status_t PollerThread::threadLoop(void* user) {
  PollerThread* const self = static_cast<PollerThread*>(user);
  auto& lock = self->mLock;
  auto& queue = self->mQueue;
  auto& cond = self->mCondition;

  LOGD("+");
  auto fetch = [&](Poller& p) -> bool {
    std::unique_lock<std::mutex> _l(lock);
    cond.wait(_l, [&] { return queue.size() > 0; });
    p = queue.front();
    queue.pop_front();
    return true;
  };

  Poller p(CMD_INVALID);
  while (fetch(p)) {
    switch (p.cmd) {
      case CMD_INIT:
        if (self->handleInit(reinterpret_cast<MessageInit*>(p.meta)) !=
            NO_ERROR) {
          LOGE("init failed");
          return -EINVAL;
        }
        break;
      case CMD_POLL:
        if (self->handlePollRequest(
                reinterpret_cast<MessagePollRequest*>(p.meta)) != NO_ERROR) {
          LOGE("PollRequest failed");
          return -EINVAL;
        }
        break;
      case CMD_FLUSH:
        if (self->handleFlush(reinterpret_cast<MessageFlush*>(p.meta)) !=
            NO_ERROR) {
          LOGE("Flush failed");
          return -EINVAL;
        }
        return NO_ERROR;
      default:
        break;
    }
  }

  LOGD("exit");
  return NO_ERROR;
}

}  // namespace v4l2
}  // namespace NSCam
