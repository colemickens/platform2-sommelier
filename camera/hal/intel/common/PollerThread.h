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

#ifndef CAMERA3_HAL_POLLERTHREAD_H_
#define CAMERA3_HAL_POLLERTHREAD_H_

#include <string>
#include <vector>

#include <cros-camera/camera_thread.h>
#include <cros-camera/v4l2_device.h>
#include <utils/Errors.h>

namespace cros {
namespace intel {

#define EVENT_POLL_TIMEOUT 100 //100 milliseconds timeout

/**
 * \class IPollEventListener
 *
 * Abstract interface implemented by entities interested on receiving notifications
 * from IPU PollerThread.
 *
 * Notifications are sent whenever the poll returns.
 */
class IPollEventListener {
public:
    enum PollEventMessageId {
        POLL_EVENT_ID_EVENT = 0,
        POLL_EVENT_ID_ERROR
    };

    struct PollEventMessageData {
        const std::vector<std::shared_ptr<cros::V4L2Device>> *activeDevices;
        const std::vector<std::shared_ptr<cros::V4L2Device>> *inactiveDevices;
        std::vector<std::shared_ptr<cros::V4L2Device>> *polledDevices; // NOTE: notified entity is allowed to change this!
        int reqId;
        int pollStatus;
    };

    struct PollEventMessage {
        PollEventMessageId id;
        PollEventMessageData data;
    };

    virtual status_t notifyPollEvent(PollEventMessage *msg) = 0;
    virtual ~IPollEventListener() {};

}; //IPollEventListener

class PollerThread
{
public:
    explicit PollerThread(std::string name);
    ~PollerThread();

    // Public Methods
    status_t init(std::vector<std::shared_ptr<cros::V4L2Device>> &devices,
                  IPollEventListener *observer,
                  int events = POLLPRI | POLLIN | POLLERR,
                  bool makeRealtime = true);
    status_t pollRequest(int reqId, int timeout = EVENT_POLL_TIMEOUT,
                         std::vector<std::shared_ptr<cros::V4L2Device>> *devices = nullptr);
    status_t flush(bool sync = false, bool clear = false);
    status_t requestExitAndWait();

//Private Members
private:
    struct MessageInit {
        IPollEventListener *observer;
        int events;
        bool makeRealtime;
        std::vector<std::shared_ptr<cros::V4L2Device>> devices;
    };

    struct MessageFlush {
        bool clearVectors;
    };

    struct MessagePollRequest {
        unsigned int reqId;
        unsigned int timeout;
        std::vector<std::shared_ptr<cros::V4L2Device>> devices;
    };

    std::vector<std::shared_ptr<cros::V4L2Device>> mPollingDevices;
    std::vector<std::shared_ptr<cros::V4L2Device>> mActiveDevices;
    std::vector<std::shared_ptr<cros::V4L2Device>> mInactiveDevices;

    std::string mName;
    cros::CameraThread mCameraThread;
    IPollEventListener* mListener; // one listener per PollerThread, PollerThread doesn't has ownership
    int mFlushFd[2];    // Flush file descriptor
    pid_t mPid;
    int mEvents;

//Private Methods
private:
    status_t handleInit(MessageInit msg);
    status_t handlePollRequest(MessagePollRequest msg);
    status_t handleFlush(MessageFlush msg);

    status_t notifyListener(IPollEventListener::PollEventMessage *msg);

};

} /* namespace intel */
} /* namespace cros */
#endif /* CAMERA3_HAL_POLLERTHREAD_H_ */
