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

class OutputFrameWorker;
class ImguUnit: public IMessageHandler,
                public IPollEventListener {

public:
    ImguUnit(int cameraId, GraphConfigManager &gcm,
            std::shared_ptr<MediaController> mediaCtl);
    virtual ~ImguUnit();
    status_t flush(void);
    status_t configStreams(std::vector<camera3_stream_t*> &activeStreams);
    void cleanListener();
    status_t completeRequest(std::shared_ptr<ProcUnitSettings> &processingSettings,
                             ICaptureEventListener::CaptureBuffers &captureBufs,
                             bool updateMeta);

    status_t attachListener(ICaptureEventListener *aListener);

    // IPollEvenListener
    virtual status_t notifyPollEvent(PollEventMessage *msg);

private:
    status_t configureVideoNodes(std::shared_ptr<GraphConfig> graphConfig);
    status_t handleMessageCompleteReq(DeviceMessage &msg);
    status_t processNextRequest();
    status_t handleMessagePoll(DeviceMessage msg);
    status_t handleMessageFlush(void);
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
    void setStreamListeners(IPU3NodeNames nodeName,
                            std::shared_ptr<OutputFrameWorker>& source);
    status_t checkAndSwitchPipe(Camera3Request* request);
    status_t kickstart();
    void clearWorkers();

    status_t allocatePublicStatBuffers(int numBufs);
    void freePublicStatBuffers();
private:
    enum ImguState {
        IMGU_RUNNING,
        IMGU_IDLE,
    };

    enum ImguPipeType {
        PIPE_VIDEO_INDEX = 0,
        PIPE_STILL_INDEX,
        PIPE_NUM
    };

    struct PipeConfiguration {
        std::vector<std::shared_ptr<IDeviceWorker>> deviceWorkers;
        std::vector<std::shared_ptr<FrameWorker>> pollableWorkers;
        std::vector<std::shared_ptr<V4L2DeviceBase>> nodes; /* PollerThread owns this */
    };

private:
    ImguState mState;

    int mCameraId;
    GraphConfigManager &mGCM;
    bool mThreadRunning;
    std::unique_ptr<MessageThread> mMessageThread;
    MessageQueue<DeviceMessage, DeviceMessageId> mMessageQueue;
    StreamConfig mActiveStreams;
    std::vector<std::shared_ptr<ITaskEventListener>> mListeningTasks;   // Tasks that listen for events from another task.

    PipeConfiguration mPipeConfigs[PIPE_NUM];
    std::vector<std::shared_ptr<IDeviceWorker>> mFirstWorkers;
    std::vector<ICaptureEventSource *> mListenerDeviceWorkers; /* mListenerDeviceWorkers doesn't own ICaptureEventSource objects */
    std::vector<ICaptureEventListener*> mListeners; /* mListeners doesn't own ICaptureEventListener objects */
    PipeConfiguration* mCurPipeConfig;

    MediaCtlHelper mMediaCtlHelper;
    std::unique_ptr<PollerThread> mPollerThread;

    std::vector<std::shared_ptr<DeviceMessage>> mMessagesPending; // Keep copy of message until workers start to handle it
    std::vector<std::shared_ptr<DeviceMessage>> mMessagesUnderwork; // Keep copy of message until workers have processed it
    std::map<IPU3NodeNames, std::shared_ptr<V4L2VideoNode>> mConfiguredNodesPerName;
    bool mFirstRequest;

    std::map<IPU3NodeNames, camera3_stream_t *> mStreamNodeMapping; /* mStreamNodeMapping doesn't own camera3_stream_t objects */
    std::map<camera3_stream_t*, IPU3NodeNames> mStreamListenerMapping;

    std::shared_ptr<SharedItemPool<ia_aiq_af_grid>> mAfFilterBuffPool; /* 3A statistics buffers */
    std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> mRgbsGridBuffPool;
    static const int PUBLIC_STATS_POOL_SIZE = 9;
    static const int IPU3_MAX_STATISTICS_WIDTH = 80;
    static const int IPU3_MAX_STATISTICS_HEIGHT = 60;

    bool mTakingPicture;
};

} /* namespace camera2 */
} /* namespace android */
#endif /* PSL_IPU3_IMGUUNIT_H_ */
