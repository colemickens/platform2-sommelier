/*
 * Copyright (C) 2017-2019 Intel Corporation.
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
#include "IPU3CameraHw.h"
#include "tasks/ICaptureEventSource.h"
#include "tasks/ExecuteTaskBase.h"
#include "workers/IDeviceWorker.h"
#include "workers/FrameWorker.h"
#include "MediaCtlHelper.h"
#include "IErrorCallback.h"
#include "workers/ParameterWorker.h"
#include <linux/intel-ipu3.h>

#include <cros-camera/camera_thread.h>

namespace cros {
namespace intel {

class OutputFrameWorker;
class ImguUnit {

public:
    ImguUnit(int cameraId, GraphConfigManager &gcm,
            std::shared_ptr<MediaController> mediaCtl,
            FaceEngine* faceEngine);
    virtual ~ImguUnit();

    void registerErrorCallback(IErrorCallback *errCb);
    status_t attachListener(ICaptureEventListener *aListener);
    void cleanListener();

    status_t configStreams(std::vector<camera3_stream_t*> &activeStreams);
    bool hasVideoPipe() { return (mImguPipe[GraphConfig::PIPE_VIDEO] != nullptr); }
    status_t completeRequest(std::shared_ptr<ProcUnitSettings> &processingSettings,
                             ICaptureEventListener::CaptureBuffers &captureBufs,
                             bool updateMeta);
    status_t flush(void);

private:
    status_t allocatePublicStatBuffers(int numBufs);
    void freePublicStatBuffers();

    class ImguPipe : public IPollEventListener {
    public:
        ImguPipe(int cameraId, GraphConfig::PipeType pipeType,
                 std::shared_ptr<MediaController> mediaCtl,
                 std::vector<ICaptureEventListener*> listeners,
                 IErrorCallback *errCb,
                 FaceEngine* faceEngine);
        virtual ~ImguPipe();

        void cleanListener();

        status_t configStreams(std::vector<camera3_stream_t*> &streams,
                               GraphConfigManager &gcm,
                               std::shared_ptr<SharedItemPool<ia_aiq_af_grid>> afGridBuffPool,
                               std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> rgbsGridBuffPool);
        status_t startWorkers();
        status_t completeRequest(std::shared_ptr<ProcUnitSettings> &processingSettings,
                                 ICaptureEventListener::CaptureBuffers &captureBufs,
                                 bool updateMeta);
        // IPollEvenListener
        virtual status_t notifyPollEvent(PollEventMessage *msg);

        status_t flush(void);

    private:
        void clearWorkers();
        status_t mapStreamWithDeviceNode(const GraphConfigManager &gcm,
                                         std::vector<camera3_stream_t*> &streams);
        status_t createProcessingTasks(std::shared_ptr<GraphConfig> graphConfig,
                                       std::shared_ptr<SharedItemPool<ia_aiq_af_grid>> afGridBuffPool,
                                       std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> rgbsGridBuffPool);

        status_t handleCompleteReq(DeviceMessage msg);
        status_t processNextRequest();
        status_t kickstart(Camera3Request* request);

        status_t startProcessing();
        status_t handlePoll(DeviceMessage msg);

        status_t updateProcUnitResults(Camera3Request &request,
                                       std::shared_ptr<ProcUnitSettings> settings);
        void updateMiscMetadata(android::CameraMetadata &result,
                                std::shared_ptr<const ProcUnitSettings> settings) const;
        void updateDVSMetadata(android::CameraMetadata &result,
                               std::shared_ptr<const ProcUnitSettings> settings) const;

        status_t handleFlush(void);

        enum ImguState {
            IMGU_RUNNING,
            IMGU_IDLE,
        };

        struct PipeConfiguration {
            std::vector<std::shared_ptr<IDeviceWorker>> deviceWorkers;
            std::vector<std::shared_ptr<FrameWorker>> pollableWorkers;
            std::vector<std::shared_ptr<cros::V4L2Device>> nodes; /* PollerThread owns this */
        };

        int mCameraId;
        GraphConfig::PipeType mPipeType;
        MediaCtlHelper mMediaCtlHelper;
        int mLastRequestId;
        ImguState mState;

        /**
         * Thread control members
         */
        cros::CameraThread mCameraThread;
        std::unique_ptr<PollerThread> mPollerThread;

        PipeConfiguration mPipeConfig;

        std::vector<std::shared_ptr<IDeviceWorker>> mFirstWorkers;
        std::vector<ICaptureEventSource *> mListenerDeviceWorkers; /* mListenerDeviceWorkers doesn't own ICaptureEventSource objects */
        std::vector<ICaptureEventListener*> mListeners; /* mListeners doesn't own ICaptureEventListener objects */

        std::vector<std::shared_ptr<DeviceMessage>> mMessagesPending; // Keep copy of message until workers start to handle it
        std::vector<std::shared_ptr<DeviceMessage>> mMessagesUnderwork; // Keep copy of message until workers have processed it
        std::map<IPU3NodeNames, std::shared_ptr<cros::V4L2VideoNode>> mConfiguredNodesPerName;
        bool mFirstRequest;
        bool mFirstPollCallbacked;
        int32_t mPollErrorTimes;
        IErrorCallback* mErrCb;
        pthread_mutex_t mFirstLock;
        pthread_cond_t mFirstCond;

        std::map<IPU3NodeNames, camera3_stream_t *> mStreamNodeMapping; /* mStreamNodeMapping doesn't own camera3_stream_t objects */
        std::map<IPU3NodeNames, std::vector<camera3_stream_t*>> mStreamListenerMapping;

        FaceEngine* mFace;
    };

    int mCameraId;
    GraphConfigManager &mGCM;
    std::shared_ptr<MediaController> mMediaCtl;
    IErrorCallback* mErrCb;
    std::shared_ptr<SharedItemPool<ia_aiq_af_grid>> mAfFilterBuffPool; /* 3A statistics buffers */
    std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> mRgbsGridBuffPool;

    static const int PUBLIC_STATS_POOL_SIZE = 9;
    static const int IPU3_MAX_STATISTICS_WIDTH = 80;
    static const int IPU3_MAX_STATISTICS_HEIGHT = 60;

    StreamConfig mActiveStreams;
    std::unique_ptr<ImguPipe> mImguPipe[GraphConfig::PIPE_MAX];

    std::vector<ICaptureEventListener*> mListeners;

    FaceEngine* mFaceEngine;
};

} /* namespace intel */
} /* namespace cros */
#endif /* PSL_IPU3_IMGUUNIT_H_ */
