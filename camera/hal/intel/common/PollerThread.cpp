/*
 * Copyright (C) 2014-2018 Intel Corporation
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

#include <unistd.h>
#include <fcntl.h>
#include "PollerThread.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"

namespace android {
namespace camera2 {

PollerThread::PollerThread(std::string name):
    mName(name),
    mCameraThread(mName.c_str()),
    mListener (nullptr),
    mEvents(POLLPRI | POLLIN | POLLERR)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);

    mFlushFd[0] = -1;
    mFlushFd[1] = -1;
    mPid = getpid();
    if (!mCameraThread.Start()) {
        LOGE("Camera thread failed to start");
    }
}

PollerThread::~PollerThread()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);

    close(mFlushFd[0]);
    close(mFlushFd[1]);
    mFlushFd[0] = -1;
    mFlushFd[1] = -1;

    // detach Listener
    mListener = nullptr;

    mCameraThread.Stop();
}

/**
 * init()
 * initialize flush file descriptors and other class members
 *
 * \param devices to poll.
 * \param observer event listener
 * \param events the poll events (bits)
 * \param makeRealtime deprecated do not use, will be removed
 * \return status
 *
 */
status_t PollerThread::init(std::vector<std::shared_ptr<cros::V4L2Device>> &devices,
                            IPollEventListener *observer,
                            int events,
                            bool makeRealtime)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
    MessageInit msg;

    msg.devices = devices; // copy the vector
    msg.observer = observer;
    msg.events = events;
    msg.makeRealtime = makeRealtime;

    status_t status = NO_ERROR;
    base::Callback<status_t()> closure =
            base::Bind(&PollerThread::handleInit, base::Unretained(this),
                       base::Passed(std::move(msg)));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

status_t PollerThread::handleInit(MessageInit msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
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
    status = fcntl(mFlushFd[0],F_SETFL,O_NONBLOCK);
    if (status < 0) {
        LOGE("Fail to set flush pipe flag: %s", strerror(errno));
        return NO_INIT;
    }

    if (msg.makeRealtime) {
        // Request to change request asynchronously
        LOGW("Real time thread priority change is not supported");
    }

    if (msg.devices.size() == 0) {
        LOGE("%s, No devices provided", __FUNCTION__);
        return BAD_VALUE;
    }

    if (msg.observer == nullptr)
    {
        LOGE("%s, No observer provided", __FUNCTION__);
        return BAD_VALUE;
    }

    mPollingDevices = msg.devices;
    mEvents = msg.events;

    //attach listener.
    mListener = msg.observer;
    return status;
}

/**
 * pollRequest()
 * this method enqueue the poll request.
 * params: request ID
 *
 */
status_t PollerThread::pollRequest(int reqId, int timeout,
                                   std::vector<std::shared_ptr<cros::V4L2Device>> *devices)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
    MessagePollRequest msg;
    msg.reqId = reqId;
    msg.timeout = timeout;
    if (devices)
        msg.devices = *devices;

    base::Callback<status_t()> closure =
            base::Bind(&PollerThread::handlePollRequest, base::Unretained(this),
                       base::Passed(std::move(msg)));
    mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
    return OK;
}

status_t PollerThread::handlePollRequest(MessagePollRequest msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
    status_t status = NO_ERROR;
    int ret;
    IPollEventListener::PollEventMessage outMsg;

    if (msg.devices.size() > 0)
        mPollingDevices = msg.devices;

    do {
        std::vector<cros::V4L2Device*> polling_devices;
        for (const auto& it : mPollingDevices) {
          polling_devices.push_back(it.get());
        }
        std::vector<cros::V4L2Device*> active_devices;
        ret = cros::V4L2DevicePoller(polling_devices, mFlushFd[0]).Poll(
                                     msg.timeout, mEvents, &active_devices);
        if (ret <= 0) {
            outMsg.id = IPollEventListener::POLL_EVENT_ID_ERROR;
        } else {
            outMsg.id = IPollEventListener::POLL_EVENT_ID_EVENT;
        }
        outMsg.data.reqId = msg.reqId;
        mActiveDevices.clear();
        mInactiveDevices.clear();
        for (const auto& it : mPollingDevices) {
          if (std::find(active_devices.begin(), active_devices.end(), it.get()) !=
              active_devices.end()) {
            mActiveDevices.push_back(it);
          }
          else {
            mInactiveDevices.push_back(it);
          }
        }
        outMsg.data.activeDevices = &mActiveDevices;
        outMsg.data.inactiveDevices = &mInactiveDevices;
        outMsg.data.polledDevices = &mPollingDevices;
        outMsg.data.pollStatus = ret;
        status = notifyListener(&outMsg);
    } while (status == -EAGAIN);
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
status_t PollerThread::flush(bool sync, bool clear)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1, LOG_TAG);
    MessageFlush msg;
    msg.clearVectors = clear;

    if (mFlushFd[1] != -1) {
        char buf = 0xf;  // random value to write to flush fd.
        unsigned int size = write(mFlushFd[1], &buf, sizeof(char));
        if (size != sizeof(char))
            LOGW("Flush write not completed");
    }

    if (sync) {
        status_t status = NO_ERROR;
        base::Callback<status_t()> closure =
                base::Bind(&PollerThread::handleFlush, base::Unretained(this),
                           base::Passed(std::move(msg)));
        mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
        return status;
    } else {
        base::Callback<status_t()> closure =
                base::Bind(&PollerThread::handleFlush, base::Unretained(this),
                           base::Passed(std::move(msg)));
        mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
        return OK;
    }
}

status_t PollerThread::handleFlush(MessageFlush msg)
{
    /**
     * read the pipe just in case there was nothing to flush.
     * this ensures that the pipe is empty for the next try.
     * this is safe because the reading end is non blocking.
     */
    if (msg.clearVectors) {
        mPollingDevices.clear();
        mActiveDevices.clear();
        mInactiveDevices.clear();
    }
    char readbuf;
    if (mFlushFd[0] != -1) {
        unsigned int size = read(mFlushFd[0], (void*) &readbuf, sizeof(char));
        if (size != sizeof(char))
            LOGW("Flush read not completed.");
    }

    return OK;
}

status_t PollerThread::requestExitAndWait(void)
{
    mCameraThread.Stop();
    return NO_ERROR;
}

/** Listener Methods **/
status_t PollerThread::notifyListener(IPollEventListener::PollEventMessage *msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
    status_t status = OK;

    if (mListener == nullptr)
        return BAD_VALUE;

    status = mListener->notifyPollEvent((IPollEventListener::PollEventMessage*)msg);

    return status;
}

} /* namespace camera2 */
} /* namespace android */
