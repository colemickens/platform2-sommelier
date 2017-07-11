/*
 * Copyright (C) 2017 Intel Corporation.
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

#ifndef PSL_IPU3_IMGUUNIT_H_
#define PSL_IPU3_IMGUUNIT_H_

#include <memory>

#include "GraphConfigManager.h"
#include "CaptureUnit.h"
#include "MessageThread.h"
#include "IPU3CameraHw.h"
#include "tasks/ITaskEventSource.h"
#include "tasks/ICaptureEventSource.h"
#include "tasks/ExecuteTaskBase.h"
#include "workers/IDeviceWorker.h"
#include "workers/FrameWorker.h"
#include "MediaCtlHelper.h"
#include "workers/ParameterWorker.h"
#include <linux/intel-ipu3.h>

namespace android {
namespace camera2 {

class ImguUnit: public IMessageHandler,
                public IPollEventListener {

public:
    ImguUnit(int cameraId, GraphConfigManager &gcm,
            std::shared_ptr<MediaController> mediaCtl);
    virtual ~ImguUnit();
    status_t flush(void);
    status_t setStillParam(ipu3_uapi_params *param);
    status_t configStreams(std::vector<camera3_stream_t*> &activeStreams);
    void cleanListener();
    status_t completeRequest(std::shared_ptr<ProcUnitSettings> &processingSettings,
                             ICaptureEventListener::CaptureBuffers &captureBufs,
                             bool updateMeta);

    status_t attachListener(ICaptureEventListener *aListener);

    // IPollEvenListener
    virtual status_t notifyPollEvent(PollEventMessage *msg);

private:
    status_t completeWaitingRequestNext(DeviceMessage &msg);

    status_t configureVideoNodes(std::shared_ptr<GraphConfig> graphConfig);
    status_t handleMessageCompleteReq(DeviceMessage &msg);
    status_t handleMessagePoll(DeviceMessage msg);
    status_t handleMessageFlush(void);
    status_t handleMessageStillParam(DeviceMessage& msg);
    status_t updateProcUnitResults(Camera3Request &request,
                                   std::shared_ptr<ProcUnitSettings> settings);
    status_t startProcessing();
    void updateMiscMetadata(CameraMetadata &result,
                            std::shared_ptr<const ProcUnitSettings> settings) const;
    void updateDVSMetadata(CameraMetadata &result,
                           std::shared_ptr<const ProcUnitSettings> settings) const;
    virtual void messageThreadLoop(void);
    status_t handleMessageExit(void);
    status_t requestExitAndWait();
    status_t mapStreamWithDeviceNode();
    status_t createProcessingTasks(std::shared_ptr<GraphConfig> graphConfig);
    status_t kickstart();

private:
    enum ImguState {
        IMGU_RUNNING,
        IMGU_IDLE,
    };


private:
    ImguState mState;

    int mCameraId;
    GraphConfigManager &mGCM;
    bool mThreadRunning;
    ParameterWorker* mParaWorker;
    std::unique_ptr<MessageThread> mMessageThread;
    MessageQueue<DeviceMessage, DeviceMessageId> mMessageQueue;
    StreamConfig mActiveStreams;
    std::vector<std::shared_ptr<ITaskEventListener>> mListeningTasks;   // Tasks that listen for events from another task.

    std::vector<std::shared_ptr<IDeviceWorker>> mDeviceWorkers;
    std::vector<std::shared_ptr<IDeviceWorker>> mFirstWorkers;
    std::vector<std::shared_ptr<FrameWorker>> mPollableWorkers;
    std::vector<ICaptureEventSource *> mListenerDeviceWorkers; /* mListenerDeviceWorkers doesn't own ICaptureEventSource objects */
    std::vector<ICaptureEventListener*> mListeners; /* mListeners doesn't own ICaptureEventListener objects */

    MediaCtlHelper mMediaCtlHelper;
    std::unique_ptr<PollerThread> mPollerThread;
    std::vector<std::shared_ptr<V4L2DeviceBase>> mNodes; /* PollerThread owns this */

    std::shared_ptr<DebugFrameRate> mFrameRateDebugger;

    std::vector<std::shared_ptr<DeviceMessage>> mMessagesUnderwork; // Keep copy of message until workers have processed it
    std::map<IPU3NodeNames, std::shared_ptr<V4L2VideoNode>> mConfiguredNodesPerName;
    bool mFirstRequest;

    std::map<IPU3NodeNames, camera3_stream_t *> mStreamNodeMapping; /* mStreamNodeMapping doesn't own camera3_stream_t objects */
};

} /* namespace camera2 */
} /* namespace android */
#endif /* PSL_IPU3_IMGUUNIT_H_ */
