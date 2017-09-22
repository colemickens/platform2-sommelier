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

#define LOG_TAG "InputSystem"

#include <string>
#include "InputSystem.h"
#include "MediaController.h"
#include "PlatformData.h"
#include "MediaEntity.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "IPU3CameraCapInfo.h"
#include "GraphConfigManager.h"
#include "GraphConfig.h"
#include "CameraStream.h"

// File-local constants
namespace {
const int ISA_CONFIG_STATS_PLANES = 2;
const int MIPI_CAPTURE_PLANES = 1;
}

namespace android {
namespace camera2 {

InputSystem::InputSystem(IISysObserver *observer, std::shared_ptr<MediaController> mediaCtl):
    mObserver(observer),
    mMediaCtl(mediaCtl),
    mMediaCtlHelper(mediaCtl, this),
    mStarted(false),
    mIsysRequestPool("IsysRequestPool"),
    mBuffersReceived(0),
    mBufferSeqNbr(0),
    mMessageQueue("Camera_InputSystem", (int)MESSAGE_ID_MAX),
    mMessageThread(new MessageThread(this, "InputSystem")),
    mThreadRunning(false),
    mPollerThread(new PollerThread("IsysPollerThread")),
    mRequestDone(true)
{
    LOG1("@%s", __FUNCTION__);
    CLEAR(mConfigResults);

    mIsysRequestPool.init(MAX_REQUEST_IN_PROCESS_NUM);
    mPendingIsysRequests.clear();
    mMessageThread->run();
}

InputSystem::~InputSystem()
{
    LOG1("@%s", __FUNCTION__);

    requestExitAndWait();

    // stop streaming before closing devices
    if (mStarted) {
        Message msg;
        CLEAR(msg);
        msg.data.stop = false;
        handleMessageStop(msg);
    }

    mConfiguredNodesPerName.clear();

    // Clear here, since mediaRequestId does not
    // make sense after nodes have been closed
    mPendingIsysRequests.clear();
}

status_t InputSystem::requestExitAndWait()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = OK;

    if (mPollerThread) {
        status |= mPollerThread->requestExitAndWait();
        mPollerThread.reset();
    }

    if (mMessageThread != nullptr) {
        Message msg;
        msg.id = MESSAGE_ID_EXIT;
        status |= mMessageQueue.send(&msg);
        status |= mMessageThread->requestExitAndWait();
        mMessageThread.reset();
        mMessageThread = nullptr;
    }

    return status;
}

status_t InputSystem::configure(IStreamConfigProvider &streamConfigMgr,
                                MediaCtlHelper::ConfigurationResults &outData)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    CLEAR(outData);
    Message msg;
    msg.id = MESSAGE_ID_CONFIGURE;
    msg.data.config.streamConfigProv = &streamConfigMgr;
    msg.data.config.result = &outData;
    status = mMessageQueue.send(&msg, MESSAGE_ID_CONFIGURE);
    return status;
}

status_t InputSystem::handleMessageConfigure(Message &msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    FrameInfo videoNodeInfo;
    IStreamConfigProvider &graphConfigMgr = *msg.data.config.streamConfigProv;
    std::shared_ptr<GraphConfig> gc = graphConfigMgr.getBaseGraphConfig();

    if (CC_UNLIKELY(gc.get() == nullptr)) {
        LOGE("ERROR: Graph config is nullptr");
        goto exit;
    }

    mConfiguredNodesPerName.clear(); // in the mMediaCtlHelper.configure, the mConfiguredNodesPerName will be triggered to be regenerated.
    mConfiguredNodes.clear();
    mStarted = false;

    status = mMediaCtlHelper.configure(graphConfigMgr, IStreamConfigProvider::CIO2);
    if (status != OK) {
        LOGE("Failed to configure input system.");
        return status;
    }

    status = gc->getSensorFrameParams(mMediaCtlHelper.getConfigResults().sensorFrameParams);
    if (status != NO_ERROR) {
        LOGE("Failed to calculate Frame Params, status:%d", status);
        goto exit;
    }

    status = mPollerThread->init((std::vector<std::shared_ptr<V4L2DeviceBase>>&) mConfiguredNodes,
                                 this, POLLPRI | POLLIN | POLLOUT | POLLERR, false);
    if (status != NO_ERROR) {
       LOGE("PollerThread init failed (ret = %d)", status);
       goto exit;
    }

    if (msg.data.config.result)
        *msg.data.config.result = mMediaCtlHelper.getConfigResults();

exit:
    mMessageQueue.reply(MESSAGE_ID_CONFIGURE, status);
    return status;
}

status_t InputSystem::opened(IPU3NodeNames isysNodeName,
        std::shared_ptr<V4L2VideoNode> videoNode)
{
    LOG1("@%s: isysNodeName:%d", __FUNCTION__, isysNodeName);
    mConfiguredNodes.push_back(videoNode);
    mConfiguredNodesPerName.insert(std::make_pair(isysNodeName,
            videoNode));

    return OK;
}

status_t InputSystem::start()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    Message msg;
    msg.id = MESSAGE_ID_START;
    status = mMessageQueue.send(&msg, MESSAGE_ID_START);

    return status;
}

status_t InputSystem::handleMessageStart()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int ret = 0;

    for (size_t i = 0; i < mConfiguredNodes.size(); i++) {
        ret = mConfiguredNodes[i]->start(0);
        if (ret < 0) {
            LOGE("STREAMON failed (%s)", mConfiguredNodes[i]->name());
            status = UNKNOWN_ERROR;
            Message msg;
            msg.id = MESSAGE_ID_STOP;
            msg.data.stop = false;
            handleMessageStop(msg);
            break;
        }
    }
    if (status == NO_ERROR)
        mStarted = true;
    mMessageQueue.reply(MESSAGE_ID_START, status);
    return status;
}

status_t InputSystem::stop(bool keepBuffers)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    Message msg;
    msg.id = MESSAGE_ID_STOP;
    msg.data.stop = keepBuffers;
    status = mMessageQueue.send(&msg, MESSAGE_ID_STOP);

    return status;
}

status_t InputSystem::handleMessageStop(Message &msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int ret = 0;
    bool keepBuffers = msg.data.stop;
    mBufferSeqNbr = 0;

    mPollerThread->flush(true);

    for (size_t i = 0; i < mConfiguredNodes.size(); i++) {
        ret = mConfiguredNodes[i]->stop(keepBuffers);
        if (ret < 0) {
            LOGE("STREAMOFF failed (%s)", mConfiguredNodes[i]->name());
            status = UNKNOWN_ERROR;
        }
    }

    // Video nodes will really stop after the buffer pools released
    if (!keepBuffers)
        mStarted = false;

    mMessageQueue.reply(MESSAGE_ID_STOP, status);
    return status;
}

status_t InputSystem::releaseBufferPools()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    Message msg;
    msg.id = MESSAGE_ID_RELEASE_BUFFER_POOLS;
    status = mMessageQueue.send(&msg, MESSAGE_ID_RELEASE_BUFFER_POOLS);

    return status;
}

status_t InputSystem::handleMessageReleaseBufferPools()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int ret = 0;

    // destroyBufferPool() is a private method in V4L2 class and
    // it is combined into stop() method. For example, mmap'ed
    // buffers require unmapping between STREAMOFF and releasing
    // buffer pool. This method allows doing these steps separately.
    for (size_t i = 0; i < mConfiguredNodes.size(); i++) {
        ret = mConfiguredNodes[i]->stop(false);
        if (ret < 0) {
            LOGE("Failed (%zu)", i);
            status = UNKNOWN_ERROR;
        }
    }

    mStarted = false;

    /*
     * Now that we are stopped we flush the poller thread to remove
     * any messages there and any references to the old nodes.
     */
    status = mPollerThread->flush(/* sync */ true, /* clear */ true);
    if (CC_UNLIKELY(status != OK)) {
        LOGW("Input system poller thread flush failed!!");
    }

    mMessageQueue.reply(MESSAGE_ID_RELEASE_BUFFER_POOLS, status);
    return status;
}

bool InputSystem::isStarted()
{
    LOG1("@%s", __FUNCTION__);

    bool value = false;
    Message msg;
    msg.id = MESSAGE_ID_IS_STARTED;
    msg.data.query.value = &value;
    mMessageQueue.send(&msg, MESSAGE_ID_IS_STARTED);

    return value;
}

status_t InputSystem::handleMessageIsStarted(Message &msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    if(msg.data.query.value)
        *msg.data.query.value = mStarted;

    mMessageQueue.reply(MESSAGE_ID_IS_STARTED, status);
    return status;
}

status_t InputSystem::putFrame(IPU3NodeNames isysNodeName,
                               const struct v4l2_buffer *buf, int32_t reqId)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    Message msg;
    msg.id = MESSAGE_ID_PUT_FRAME;
    msg.data.frame.reqId = reqId;
    msg.data.frame.isysNodeName = isysNodeName;
    msg.data.frame.buf = buf;

    status = mMessageQueue.send(&msg);

    return status;
}

status_t InputSystem::handleMessagePutFrame(Message &msg)
{
    LOG2("@%s", __FUNCTION__);

    std::shared_ptr<IsysRequest> isysReq;
    status_t status = NO_ERROR;
    bool newReq = false;
    IPU3NodeNames isysNodeName = msg.data.frame.isysNodeName;
    const struct v4l2_buffer *buf = msg.data.frame.buf;
    const int32_t reqId = msg.data.frame.reqId;

    /* first checking if existing mediaRequest created */
    auto itRequest = mPendingIsysRequests.find(reqId);
    if (itRequest == mPendingIsysRequests.end()) {
        // create
        LOG2("%s: create new pending Isys Request for reqId %d", __FUNCTION__,
                                                                 reqId);
        status = mIsysRequestPool.acquireItem(isysReq);
        if (status != NO_ERROR) {
            LOGE("failed to acquire Isys Request");
            return UNKNOWN_ERROR;
        }
        if (isysReq == nullptr) {
            LOGE("failed to acquire Isys Request(nullptr)");
            return UNKNOWN_ERROR;
        }

        // clear potential reused Nodes
        isysReq->configuredNodesForRequest.clear();
        newReq = true;
    } else {
        // edit
        LOG2("%s: Found Pending Request for ReqId %d", __FUNCTION__, reqId);
        isysReq = mPendingIsysRequests.at(reqId);
    }

    ConfiguredNodesPerName::iterator it =
                            mConfiguredNodesPerName.find(isysNodeName);
    if (it == mConfiguredNodesPerName.end()) {
        LOGE("ISYS putframe - node (%d) not found!", isysNodeName);
        return BAD_VALUE;
    }
    std::shared_ptr<V4L2VideoNode> videoNode = it->second;
    int ret = videoNode->putFrame(buf);
    if (ret < 0) {
        LOGE("isys putframe failed for dev: %s", videoNode->name());
        return UNKNOWN_ERROR;
    }

    isysReq->configuredNodesForRequest.push_back(videoNode);
    isysReq->numNodesForRequest = isysReq->configuredNodesForRequest.size();
    if (newReq) {
        isysReq->mediaRequestId = 44; // remove
        isysReq->requestId = msg.data.frame.reqId;
        mPendingIsysRequests.emplace(isysReq->requestId, isysReq);
    }

    return NO_ERROR;
}

status_t InputSystem::grabFrame(IPU3NodeNames isysNodeName, struct v4l2_buffer_info *buf)
{
    LOG2("@%s", __FUNCTION__);
    ConfiguredNodesPerName::iterator it =
                            mConfiguredNodesPerName.find(isysNodeName);
    if (it == mConfiguredNodesPerName.end()) {
        LOGE("ISYS node (%d) not found!", isysNodeName);
        return BAD_VALUE;
    }
    std::shared_ptr<V4L2VideoNode> videoNode = it->second;
    int ret = videoNode->grabFrame(buf);
    if (ret < 0) {
        LOGE("@%s failed", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t InputSystem::setBufferPool(IPU3NodeNames isysNodeName,
                                    std::vector<struct v4l2_buffer> &pool,
                                    bool cached)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    Message msg;
    msg.id = MESSAGE_ID_SET_BUFFER_POOL;
    msg.data.bufferPool.isysNodeName = isysNodeName;
    msg.data.bufferPool.pool = &pool;
    msg.data.bufferPool.cached = cached;
    status = mMessageQueue.send(&msg, MESSAGE_ID_SET_BUFFER_POOL);

    return status;
}

status_t InputSystem::handleMessageSetBufferPool(Message &msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    IPU3NodeNames isysNodeName = msg.data.bufferPool.isysNodeName;
    std::vector<struct v4l2_buffer> *pool = msg.data.bufferPool.pool;
    bool cached = msg.data.bufferPool.cached;
    std::shared_ptr<V4L2VideoNode> videoNode = nullptr;

    int memType = getDefaultMemoryType(ISYS_NODE_RAW);

    ConfiguredNodesPerName::iterator it =
                            mConfiguredNodesPerName.find(isysNodeName);
    if (it == mConfiguredNodesPerName.end()) {
        LOGE("@%s: ISYS node (%d) not found!", __FUNCTION__, isysNodeName);
        status = BAD_VALUE;
        goto exit;
    }
    videoNode = it->second;
    status = videoNode->setBufferPool(*pool, cached, memType);
    if (status != NO_ERROR) {
        LOGE("Failed setting buffer poll into the device.");
        goto exit;
    }

exit:
    mMessageQueue.reply(MESSAGE_ID_SET_BUFFER_POOL, status);
    return NO_ERROR;
}

status_t InputSystem::getOutputNodes(ConfiguredNodesPerName **nodes,
                                            int &nodeCount)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    Message msg;
    msg.id = MESSAGE_ID_GET_NODES;
    msg.data.nodes.nodes = nodes;
    msg.data.nodes.nodeCount = &nodeCount;
    status = mMessageQueue.send(&msg, MESSAGE_ID_GET_NODES);
    return status;
}

std::shared_ptr<V4L2VideoNode> InputSystem::findOutputNode(IPU3NodeNames isysNodeName)
{
    auto it = mConfiguredNodesPerName.find(isysNodeName);
    if (it == mConfiguredNodesPerName.end()) {
        LOGE("@%s ISYS node (%d) not found!", __FUNCTION__, isysNodeName);
        return nullptr;
    }
    return it->second;
}

status_t InputSystem::handleMessageGetOutputNodes(Message &msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    ConfiguredNodesPerName **nodes = msg.data.nodes.nodes;
    int *nodeCount = msg.data.nodes.nodeCount;

    if (nodes)
        *nodes = &mConfiguredNodesPerName;

    if (nodeCount)
        *nodeCount = mConfiguredNodes.size();

    mMessageQueue.reply(MESSAGE_ID_GET_NODES, status);
    return status;
}

status_t InputSystem::enqueueMediaRequest(int32_t reqId)
{
    LOG2("@%s, reqId = %d", __FUNCTION__, reqId);
    status_t status = NO_ERROR;
    Message msg;
    msg.id = MESSAGE_ID_ENQUEUE_MEDIA_REQUEST;
    msg.data.enqueueMediaRequest.requestId = reqId;
    status = mMessageQueue.send(&msg);

    return status;
}

status_t InputSystem::handleMessageEnqueueMediaRequest(Message &msg)
{
    LOG2("@%s", __FUNCTION__);
    int32_t reqId = msg.data.enqueueMediaRequest.requestId;
    std::shared_ptr<IsysRequest> currentRequest = nullptr;
    /* first checking if existing mediaRequest created */
    auto itRequest = mPendingIsysRequests.find(reqId);
    if (itRequest == mPendingIsysRequests.end()) {
        LOGE("No request pending for reqId %d, BUG!", reqId);
        return UNKNOWN_ERROR;
    }

    currentRequest = mPendingIsysRequests.at(reqId);
    mMediaCtl->enqueueMediaRequest(currentRequest->mediaRequestId);
    return OK;
}

status_t InputSystem::capture(int requestId)
{
    LOG2("@%s: request ID: %d", __FUNCTION__, requestId);
    status_t status = NO_ERROR;

    Message msg;
    msg.id = MESSAGE_ID_CAPTURE;
    msg.data.capture.requestId = requestId;

    status = mMessageQueue.send(&msg);

    return status;
}

status_t InputSystem::handleMessageCapture(Message &msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    mCaptureQueue.push_back(msg.data.capture.requestId);
    // Start polling if all buffers for the previous request have
    // been received
    if (mRequestDone) {
        pollNextRequest();
        mRequestDone = false;
    }
    return status;
}

status_t InputSystem::flush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // flush the poll messages
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    mMessageQueue.remove(MESSAGE_ID_CAPTURE);
    status = mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
    return status;
}

status_t InputSystem::handleMessageFlush()
{
    LOG1("@%s:", __FUNCTION__);
    status_t status = NO_ERROR;
    mPollerThread->flush(true);
    mMessageQueue.reply(MESSAGE_ID_FLUSH, status);
    return status;
}

void InputSystem::messageThreadLoop(void)
{
    LOG1("@%s: Start", __FUNCTION__);

    mThreadRunning = true;
    while (mThreadRunning) {
        status_t status = NO_ERROR;

        Message msg;
        mMessageQueue.receive(&msg);

        PERFORMANCE_HAL_ATRACE_PARAM1("msg", msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
            mThreadRunning = false;
            mStarted = false;
            status = NO_ERROR;
            break;

        case MESSAGE_ID_CONFIGURE:
            status = handleMessageConfigure(msg);
            break;

        case MESSAGE_ID_START:
            status = handleMessageStart();
            break;

        case MESSAGE_ID_STOP:
            status = handleMessageStop(msg);
            break;

        case MESSAGE_ID_IS_STARTED:
            status = handleMessageIsStarted(msg);
            break;

        case MESSAGE_ID_PUT_FRAME:
            status = handleMessagePutFrame(msg);
            break;

        case MESSAGE_ID_SET_BUFFER_POOL:
            status = handleMessageSetBufferPool(msg);
            break;

        case MESSAGE_ID_GET_NODES:
            status = handleMessageGetOutputNodes(msg);
            break;

        case MESSAGE_ID_ENQUEUE_MEDIA_REQUEST:
            status =  handleMessageEnqueueMediaRequest(msg);
            break;

        case MESSAGE_ID_CAPTURE:
            status = handleMessageCapture(msg);
            break;

        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;

        case MESSAGE_ID_POLL:
            status = handleMessagePollEvent(msg);
            break;

        case MESSAGE_ID_RELEASE_BUFFER_POOLS:
            status = handleMessageReleaseBufferPools();
            break;

        default:
            LOGE("@%s: Unknown message: %d", __FUNCTION__, msg.id);
            status = BAD_VALUE;
            break;
        }
        if (status != NO_ERROR)
            LOGE("error %d in handling message: %d", status, (int)msg.id);

    }
    LOG1("%s: Exit", __FUNCTION__);
}

status_t InputSystem::notifyPollEvent(PollEventMessage *pollMsg)
{
    LOG2("@%s", __FUNCTION__);

    if (pollMsg == nullptr || pollMsg->data.activeDevices == nullptr)
        return BAD_VALUE;

    // Common thread message fields for any case
    Message msg;
    msg.id = MESSAGE_ID_POLL;
    msg.data.pollEvent.pollMsgId = pollMsg->id;
    msg.data.pollEvent.requestId = pollMsg->data.reqId;

    if (pollMsg->id == POLL_EVENT_ID_EVENT) {
        int numDevices = pollMsg->data.activeDevices->size();
        if (numDevices == 0) {
            LOG1("@%s: devices flushed", __FUNCTION__);
            return OK;
        }

        int numPolledDevices = pollMsg->data.polledDevices->size();
        if (CC_UNLIKELY(numPolledDevices == 0)) {
            LOGW("No devices Polled?");
            return OK;
        }

        msg.data.pollEvent.activeDevices = new std::shared_ptr<V4L2VideoNode>[numDevices];
        for (int i = 0; i < numDevices; i++) {
            msg.data.pollEvent.activeDevices[i] = (std::shared_ptr<V4L2VideoNode>&) pollMsg->data.activeDevices->at(i);
        }
        msg.data.pollEvent.numDevices = numDevices;
        msg.data.pollEvent.polledDevices = numPolledDevices;

        mMessageQueue.send(&msg);

        if (pollMsg->data.activeDevices->size() != pollMsg->data.polledDevices->size()) {
            LOG2("@%s: %lu inactive nodes for request %d, retry poll", __FUNCTION__,
                                                                      pollMsg->data.inactiveDevices->size(),
                                                                      pollMsg->data.reqId);
            pollMsg->data.polledDevices->clear();
            *pollMsg->data.polledDevices = *pollMsg->data.inactiveDevices; // retry with inactive devices
            return -EAGAIN;
        }
    } else if (pollMsg->id == POLL_EVENT_ID_ERROR) {
        LOGE("device poll failed");
        // For now, set number of device to zero in error case
        msg.data.pollEvent.numDevices = 0;
        msg.data.pollEvent.polledDevices = 0;
        mMessageQueue.send(&msg);
    } else {
        LOGW("unknown poll event id (%d)", pollMsg->id);
    }

    return OK;
}

status_t InputSystem::getIsysNodeName(std::shared_ptr<V4L2VideoNode> node, IPU3NodeNames &isysNodeName)
{
    LOG2("@%s", __FUNCTION__);

    for (const auto &configNode : mConfiguredNodesPerName) {
        if (configNode.second->getFd() == node->getFd()) {
            isysNodeName = configNode.first;
            return NO_ERROR;
        }
    }

    return BAD_VALUE;
}

status_t InputSystem::handleMessagePollEvent(Message &msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    struct v4l2_buffer_info outBuf;
    std::shared_ptr<V4L2VideoNode> *activeNodes;
    IPU3NodeNames isysNodeName = IMGU_NODE_NULL;
    uint8_t nodeCount = mCaptureInProgress->numNodesForRequest;
    int activeNodecount = 0;
    int requestId = -999;

    CLEAR(outBuf);
    activeNodes = msg.data.pollEvent.activeDevices;
    activeNodecount = msg.data.pollEvent.numDevices;
    requestId = msg.data.pollEvent.requestId;

    LOG2("@%s: received %d / %d buffers for request Id %d", __FUNCTION__,
                                                            activeNodecount,
                                                            nodeCount,
                                                            requestId);

    if (msg.data.pollEvent.pollMsgId == POLL_EVENT_ID_ERROR) {
        // Notify observer
        IISysObserver::IsysMessage isysMsg;
        isysMsg.id = IISysObserver::ISYS_MESSAGE_ID_ERROR;
        isysMsg.data.error.status = status;
        mObserver->notifyIsysEvent(isysMsg);
        // Poll again
        status = mPollerThread->pollRequest(mCaptureInProgress->requestId,
                   IPU3_EVENT_POLL_TIMEOUT,
                   (std::vector<std::shared_ptr<V4L2DeviceBase>>*) &mCaptureInProgress->configuredNodesForRequest);
        return status;
    }

    for (int i = 0; i < activeNodecount; i++) {
        status = getIsysNodeName(activeNodes[i], isysNodeName);
        if (status != NO_ERROR) {
            LOGE("Error getting ISYS node, err %d", status);
            // Notify observer
            IISysObserver::IsysMessage isysMsg;
            isysMsg.id = IISysObserver::ISYS_MESSAGE_ID_ERROR;
            isysMsg.data.error.status = status;
            mObserver->notifyIsysEvent(isysMsg);
            // Poll again
            status = mPollerThread->pollRequest(mCaptureInProgress->requestId,
                       IPU3_EVENT_POLL_TIMEOUT,
                       (std::vector<std::shared_ptr<V4L2DeviceBase>>*) &mCaptureInProgress->configuredNodesForRequest);
            return status;
        }

        status = grabFrame(isysNodeName, &outBuf);

        if (status != NO_ERROR) {
            LOGE("Error getting data from ISYS node %d", isysNodeName);
            // Notify observer
            IISysObserver::IsysMessage isysMsg;
            isysMsg.id = IISysObserver::ISYS_MESSAGE_ID_ERROR;
            isysMsg.data.error.status = status;
            mObserver->notifyIsysEvent(isysMsg);
            // Poll again
            status = mPollerThread->pollRequest(mCaptureInProgress->requestId,
                       IPU3_EVENT_POLL_TIMEOUT,
                       (std::vector<std::shared_ptr<V4L2DeviceBase>>*) &mCaptureInProgress->configuredNodesForRequest);
            return status;
        }

        // When receiving the first buffer for a request,
        // store the sequence number. All buffers should
        // have the same sequence number.
        if (mBufferSeqNbr == 0) {
            mBufferSeqNbr = outBuf.vbuffer.sequence;
        } else if (mBufferSeqNbr != outBuf.vbuffer.sequence) {
            LOGW("Sequence number mismatch, expecting %d but received %d",
                  mBufferSeqNbr, outBuf.vbuffer.sequence);
            mBufferSeqNbr = outBuf.vbuffer.sequence;
        }
        LOG2("input system outBuf.vbuffer.sequence %u", outBuf.vbuffer.sequence);
        mBuffersReceived++;

        // Notify observer
        IISysObserver::IsysMessage isysMsg;
        isysMsg.id = IISysObserver::ISYS_MESSAGE_ID_EVENT;
        isysMsg.data.event.requestId = requestId;
        isysMsg.data.event.isysNodeName = isysNodeName;
        isysMsg.data.event.buffer = &outBuf;
        mObserver->notifyIsysEvent(isysMsg);

    }
    delete[] activeNodes;

    if (mBuffersReceived == nodeCount) {
        LOG2("@%s: all buffers received (%d/%d) for request Id %d",
             __FUNCTION__, mBuffersReceived, nodeCount, requestId);
        mBuffersReceived = 0;
        mBufferSeqNbr++;

        mRequestDone = true;

        if (!mStarted)
            return status;

        // Start polling if there is a new request in the queue
        if (mPendingIsysRequests.size() > 0 && mCaptureQueue.size() > 0) {
            pollNextRequest();

            mRequestDone = false;
        }
    }
    return status;
}

status_t InputSystem::pollNextRequest()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = NO_ERROR;
    int reqId = mCaptureQueue[0];
    mCaptureInProgress.reset();
    /* first checking if existing mediaRequest created */
    auto itRequest = mPendingIsysRequests.find(reqId);
    if (itRequest == mPendingIsysRequests.end()) {
        LOGE("No Request pending for reqId %d, BUG!", reqId);
        return UNKNOWN_ERROR;
    }
    mCaptureInProgress = mPendingIsysRequests.at(reqId);
    // clean queue of request.
    mPendingIsysRequests.erase(reqId);
    mCaptureQueue.erase(mCaptureQueue.begin());

    status = mPollerThread->pollRequest(mCaptureInProgress->requestId,
                                        IPU3_EVENT_POLL_TIMEOUT,
                                        (std::vector<std::shared_ptr<V4L2DeviceBase>>*) &mCaptureInProgress->configuredNodesForRequest);

    return status;
}

}
}
