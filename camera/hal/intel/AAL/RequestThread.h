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

#ifndef _CAMERA3_REQUESTTHREAD_H_
#define _CAMERA3_REQUESTTHREAD_H_

#include "ResultProcessor.h"
#include "ItemPool.h"
#include <hardware/camera3.h>

#include <base/synchronization/waitable_event.h>
#include <cros-camera/camera_thread.h>

NAMESPACE_DECLARATION {

/**
 * \class RequestThread
 * Active object in charge of request management
 *
 * The RequestThread  is the in charge of controlling the flow of request from
 * the client to the HW class.
 */
class RequestThread {
public:
    RequestThread(int cameraId, ICameraHw *aCameraHW);
    virtual ~RequestThread();

    status_t init(const camera3_callback_ops_t *callback_ops);
    status_t deinit(void);
    status_t run();

    status_t configureStreams(camera3_stream_configuration_t *stream_list);
    status_t constructDefaultRequest(int type, camera_metadata_t** meta);
    status_t processCaptureRequest(camera3_capture_request_t *request);
    status_t flush();

    int returnRequest(Camera3Request* req);

    void dump(int fd);

    enum RequestBlockAction {
        REQBLK_NONBLOCKING = NO_ERROR,      /* request is non blocking */
        REQBLK_WAIT_ALL_PREVIOUS_COMPLETED, /* wait all previous requests completed */
        REQBLK_WAIT_ONE_REQUEST_COMPLETED,  /* the count of request in process reached the max,
                                              wait at least one request is completed */
        REQBLK_UNKOWN_ERROR,                /* unknow issue */
    };

private:  /* types */
    struct MessageConfigureStreams {
        camera3_stream_configuration_t * list;
    };

    struct MessageConstructDefaultRequest {
        int type;
        camera_metadata_t ** request;
    };

    struct MessageProcessCaptureRequest {
        camera3_capture_request * request3;
    };

    struct MessageStreamOutDone {
        int reqId;
        Camera3Request *request;
    };

private:  /* methods */
    status_t handleConfigureStreams(MessageConfigureStreams msg);
    status_t handleConstructDefaultRequest(MessageConstructDefaultRequest msg);
    status_t handleProcessCaptureRequest(MessageProcessCaptureRequest msg);
    status_t handleReturnRequest(MessageStreamOutDone msg);
    status_t handleExit();

    status_t captureRequest(Camera3Request* request);

    bool areAllStreamsUnderMaxBuffers() const;
    void deleteStreams(bool inactiveOnly);

private:  /* members */
    int mCameraId;
    ICameraHw   *mCameraHw; /* allocate from outside and should not delete in here */
    ItemPool<Camera3Request> mRequestsPool;

    int mRequestsInHAL;
    Camera3Request* mWaitingRequest;  /*!< storage during need to wait for
                                           captures to be finished.
                                           It is one item from mRequestsPool */
    int mBlockAction;   /*!< the action if request is blocked */
    CameraMetadata mLastSettings;

    bool mInitialized;  /*!< tracking the status of the RequestThread */
    /* *********************************************************************
     * Stream info
     */
    ResultProcessor* mResultProcessor;
    std::vector<camera3_stream_t *> mStreams; /* Map to camera3_stream_t from framework
                                              which are not allocated here */
    std::vector<CameraStream*> mLocalStreams; /* Local storage of streaming informations */
    unsigned int mStreamSeqNo;

    arc::CameraThread mCameraThread;

    base::WaitableEvent mWaitRequest; /* Guide blocking capture request */
};

} NAMESPACE_DECLARATION_END
#endif /* _CAMERA3_REQUESTTHREAD_H_ */
