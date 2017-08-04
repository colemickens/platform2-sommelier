/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#include "MessageQueue.h"
#include "MessageThread.h"
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
#include "IMGUTypes.h"

/**
 * Forward declarations to avoid includes
 */
class MediaController;
class MediaEntity;

namespace android {
namespace camera2 {

const int IPU3_EVENT_POLL_TIMEOUT = 10000;  // msecs

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
        struct v4l2_buffer_info *buffer;
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

class InputSystem:  public IMessageHandler,
                    public IPollEventListener,
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

    status_t putFrame(IPU3NodeNames isysNodeName, const struct v4l2_buffer *buf, int32_t reqId);
    status_t setBufferPool(IPU3NodeNames isysNodeName, std::vector<struct v4l2_buffer> &pool, bool cached);
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
    status_t grabFrame(IPU3NodeNames isysNodeName, struct v4l2_buffer_info *buf);
    status_t pollNextRequest();
    status_t getIsysNodeName(std::shared_ptr<V4L2VideoNode> node, IPU3NodeNames &isysNodeName);

    // thread message IDs
    enum MessageId {
        MESSAGE_ID_EXIT = 0,
        MESSAGE_ID_CONFIGURE,
        MESSAGE_ID_START,
        MESSAGE_ID_STOP,
        MESSAGE_ID_IS_STARTED,
        MESSAGE_ID_PUT_FRAME,
        MESSAGE_ID_SET_BUFFER_POOL,
        MESSAGE_ID_GET_NODES,
        MESSAGE_ID_ENQUEUE_MEDIA_REQUEST,
        MESSAGE_ID_CAPTURE,
        MESSAGE_ID_FLUSH,
        MESSAGE_ID_POLL,
        MESSAGE_ID_RELEASE_BUFFER_POOLS,
        MESSAGE_ID_MAX
    };

    struct MessageConfigure {
        IStreamConfigProvider *streamConfigProv;
        MediaCtlHelper::ConfigurationResults *result;
    };

    struct MessageFrame {
        int32_t reqId;
        IPU3NodeNames isysNodeName;
        const struct v4l2_buffer *buf;
    };

    struct MessageBufferPool {
        IPU3NodeNames isysNodeName;
        std::vector<struct v4l2_buffer> *pool;
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

    struct MessagePSysStats {
        int requestId;
        CaptureBuffer*  rawBuffer;
        CaptureBuffer*  yuvBuffer;
        CaptureBuffer*  statsBuffer;
    };

    struct MessagePollEvent {
        int requestId;
        std::shared_ptr<V4L2VideoNode> *activeDevices;
        int polledDevices;
        int numDevices;
        PollEventMessageId pollMsgId;
    };

    struct MessageIsaConfig {
        int enabledBlocks;
        int pgSize;
        int payloadSize;
    };

    union MessageData {
        MessageFrame                    frame;
        MessageBufferPool               bufferPool;
        MessageNodes                    nodes;
        MessageConfigure                config;
        MessageEnqueueMediaRequest      enqueueMediaRequest;
        MessageCapture                  capture;
        MessagePollEvent                pollEvent;
        MessageIsaConfig                isaConfig;
        bool                            stop;
        MessageBoolQuery                query;
    };

    struct Message {
        MessageId id;
        MessageData data;
    };

    /* IMessageHandler overloads */
    virtual void messageThreadLoop(void);

    status_t handleMessageConfigure(Message &msg);
    status_t handleMessageStart();
    status_t handleMessageStop(Message &msg);
    status_t handleMessageIsStarted(Message &msg);
    status_t handleMessagePutFrame(Message &msg);
    status_t handleMessageSetBufferPool(Message &msg);
    status_t handleMessageReleaseBufferPools();
    status_t handleMessageGetOutputNodes(Message &msg);
    status_t handleMessageEnqueueMediaRequest(Message &msg);
    status_t handleMessageCapture(Message &msg);
    status_t handleMessagePollEvent(Message &msg);
    status_t handleMessageFlush();
    status_t handleMessageSetIsaConfig(Message &msg);
    status_t handleMessageSetIsaFormat(Message &msg);
    status_t handleMessageSetStatsFormat(Message &msg);

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
    MessageQueue<Message, MessageId> mMessageQueue;
    std::shared_ptr<MessageThread> mMessageThread;
    bool mThreadRunning;

    std::unique_ptr<PollerThread> mPollerThread;
    std::vector<int> mCaptureQueue;
    std::shared_ptr<IsysRequest> mCaptureInProgress;
    friend class std::shared_ptr<IsysRequest>;
    bool mRequestDone;

}; // class InputSystem

} // namespace camera2
} // namespace android

#endif
