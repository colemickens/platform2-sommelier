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

#ifndef CAMERA3_HAL_CSS2600ISYS_H_
#define CAMERA3_HAL_CSS2600ISYS_H_

#include "PollerThread.h"
#include "v4l2device.h"
#include "MediaCtlPipeConfig.h"
#include <linux/media.h>
#include <poll.h>
#include <vector>
#include <map>
#include "ItemPool.h"
#include "Intel3aPlus.h"
#include "CaptureBuffer.h"
#include "SyncManager.h"
#include "SharedItemPool.h"
#include <utils/Errors.h>
#include "MediaCtlHelper.h"
#include "NodeTypes.h"

#include <arc/camera_thread.h>

/**
 * Forward declarations to avoid includes
 */
class MediaController;
class MediaEntity;

namespace android {
namespace camera2 {

class IStreamConfigProvider;

class IISysObserver {
public:
    enum IsysMessageId {
        ISYS_MESSAGE_ID_EVENT = 0,
        ISYS_MESSAGE_ID_ERROR
    };

    // For MESSAGE_ID_EVENT
    struct IsysMessageEvent {
        int requestId;
        IPU3NodeNames isysNodeName;
        V4L2BufferInfo *buffer;
    };

    // For MESSAGE_ID_ERROR
    struct IsysMessageError {
        status_t status;
    };

    struct IsysMessageData {
        IsysMessageEvent event;
        IsysMessageError error;
    };

    struct IsysMessage {
        IsysMessageId   id;
        IsysMessageData data;
    };

    virtual void notifyIsysEvent(IsysMessage &msg) = 0;
    virtual ~IISysObserver() {};
};

class InputSystem : public IPollEventListener,
                    public ISettingsSyncListener,
                    public MediaCtlHelper::IOpenCallBack
{
public: /* types */
   typedef std::map<IPU3NodeNames, std::shared_ptr<V4L2VideoNode>> ConfiguredNodesPerName;

public:
    InputSystem(IISysObserver *observer, std::shared_ptr<MediaController> mediaCtl);
    ~InputSystem();

    status_t configure(IStreamConfigProvider &streamConfigProv,
                       MediaCtlHelper::ConfigurationResults &outData);

    status_t start();
    status_t stop(bool keepBuffers = false);
    bool isStarted();

    status_t putFrame(IPU3NodeNames isysNodeName, const V4L2Buffer *buf, int32_t reqId);
    status_t setBufferPool(IPU3NodeNames isysNodeName, std::vector<V4L2Buffer> &pool, bool cached);
    status_t releaseBufferPools();
    status_t getOutputNodes(ConfiguredNodesPerName **nodes, int &nodeCount);
    std::shared_ptr<V4L2VideoNode> findOutputNode(IPU3NodeNames isysNodeName);
    status_t enqueueMediaRequest(int32_t requestId);
    status_t capture(int requestId);
    status_t flush();
    // IPollEvenListener
    virtual status_t notifyPollEvent(PollEventMessage *msg);

    status_t setIsaConfig(int enabledBlocks);
    status_t setIsaFormat(int pgSize, int payloadSize);
    status_t setStatsFormat(int pgSize, int payloadSize);
    status_t requestExitAndWait();

private: /* methods */
    status_t grabFrame(IPU3NodeNames isysNodeName, V4L2BufferInfo *buf);
    status_t pollNextRequest();
    status_t getIsysNodeName(std::shared_ptr<V4L2VideoNode> node, IPU3NodeNames &isysNodeName);

    struct MessageConfigure {
        IStreamConfigProvider *streamConfigProv;
        MediaCtlHelper::ConfigurationResults *result;
    };

    struct MessageFrame {
        int32_t reqId;
        IPU3NodeNames isysNodeName;
        const V4L2Buffer *buf;
    };

    struct MessageBufferPool {
        IPU3NodeNames isysNodeName;
        std::vector<V4L2Buffer> *pool;
        bool cached;
    };

    struct MessageNodes {
        ConfiguredNodesPerName **nodes;
        int *nodeCount;
    };

    struct MessageEnqueueMediaRequest {
        int32_t requestId;
    };

    struct MessageCapture {
        int requestId;
    };

    struct MessageBoolQuery {
        bool *value;
    };

    struct MessagePollEvent {
        int requestId;
        std::shared_ptr<V4L2VideoNode> *activeDevices;
        int polledDevices;
        int numDevices;
        PollEventMessageId pollMsgId;
    };

    struct MessageStop {
        bool stop;
    };

    status_t handleConfigure(MessageConfigure msg);
    status_t handleStart();
    status_t handleStop(MessageStop msg);
    status_t handleIsStarted(MessageBoolQuery msg);
    status_t handlePutFrame(MessageFrame msg);
    status_t handleSetBufferPool(MessageBufferPool msg);
    status_t handleReleaseBufferPools();
    status_t handleGetOutputNodes(MessageNodes msg);
    status_t handleEnqueueMediaRequest(MessageEnqueueMediaRequest msg);
    status_t handleCapture(MessageCapture msg);
    status_t handlePollEvent(MessagePollEvent msg);
    status_t handleFlush();
    status_t handleExit();

    status_t opened(IPU3NodeNames isysNodeName,
                    std::shared_ptr<V4L2VideoNode> videoNode);
private: /* members */
    IISysObserver*      mObserver; /* InputSystem doesn't own mObserver */
    std::shared_ptr<MediaController> mMediaCtl;
    MediaCtlHelper mMediaCtlHelper;

    bool                mStarted;

    std::vector<std::shared_ptr<V4L2VideoNode>>  mConfiguredNodes;        /**< Configured video nodes */
    ConfiguredNodesPerName mConfiguredNodesPerName; /**< Configured video nodes, Key: ISYS node name */

    struct IsysRequest {
        int32_t requestId;
        uint32_t mediaRequestId;
        uint8_t numNodesForRequest;
        std::vector<std::shared_ptr<V4L2VideoNode>> configuredNodesForRequest; /**< Configured video Node for request */
    };

    SharedItemPool<IsysRequest>  mIsysRequestPool;
    std::map<int32_t, std::shared_ptr<IsysRequest>> mPendingIsysRequests;

    MediaCtlHelper::ConfigurationResults  mConfigResults;
    uint8_t               mBuffersReceived;
    uint32_t              mBufferSeqNbr;

    /**
     * Thread control members
     */
    arc::CameraThread mCameraThread;

    std::unique_ptr<PollerThread> mPollerThread;
    std::vector<int> mCaptureQueue;
    std::shared_ptr<IsysRequest> mCaptureInProgress;
    friend class std::shared_ptr<IsysRequest>;
    bool mRequestDone;

}; // class InputSystem

} // namespace camera2
} // namespace android

#endif
