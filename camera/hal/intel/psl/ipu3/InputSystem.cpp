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
    mCameraThread("InputSystem"),
    mPollerThread(new PollerThread("IsysPollerThread")),
    mRequestDone(true),
    mPollErrorTimes(0),
    mErrCb(nullptr)

{
    LOG1("@%s", __FUNCTION__);
    mIsysRequestPool.init(MAX_REQUEST_IN_PROCESS_NUM);
    mPendingIsysRequests.clear();
    if (!mCameraThread.Start()) {
        LOGE("Camera thread failed to start");
    }
}

InputSystem::~InputSystem()
{
    LOG1("@%s", __FUNCTION__);

    requestExitAndWait();

    mConfiguredNodesPerName.clear();

    // Clear here, since mediaRequestId does not
    // make sense after nodes have been closed
    mPendingIsysRequests.clear();
}

status_t InputSystem::requestExitAndWait()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = OK;

    if (mStarted) {
        status_t ret = NO_ERROR;
        base::Callback<status_t()> closure =
                base::Bind(&InputSystem::handleExit, base::Unretained(this));
        mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &ret);
        status |= ret;
    }

    if (mPollerThread) {
        status |= mPollerThread->requestExitAndWait();
        mPollerThread.reset();
    }
    mCameraThread.Stop();
    return status;
}

void InputSystem::registerErrorCallback(IErrorCallback *errCb)
{
    LOG1("@%s, errCb:%p", __FUNCTION__, errCb);
    mErrCb = errCb;
}

status_t InputSystem::configure(IStreamConfigProvider &streamConfigMgr,
                                MediaCtlHelper::ConfigurationResults &outData)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    CLEAR(outData);
    MessageConfigure msg;
    msg.streamConfigProv = &streamConfigMgr;
    msg.result = &outData;
    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleConfigure, base::Unretained(this),
                       base::Passed(std::move(msg)));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

status_t InputSystem::handleConfigure(MessageConfigure msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    FrameInfo videoNodeInfo;
    IStreamConfigProvider &graphConfigMgr = *msg.streamConfigProv;
    std::shared_ptr<GraphConfig> gc = graphConfigMgr.getBaseGraphConfig(IStreamConfigProvider::CIO2);

    if (CC_UNLIKELY(gc.get() == nullptr)) {
        LOGE("ERROR: Graph config is nullptr");
        return status;
    }

    mConfiguredNodesPerName.clear(); // in the mMediaCtlHelper.configure, the mConfiguredNodesPerName will be triggered to be regenerated.
    mConfiguredNodes.clear();
    mStarted = false;

    mMediaCtlHelper.closeVideoNodes();
    mMediaCtlHelper.resetLinks();
    status = mMediaCtlHelper.configure(graphConfigMgr, IStreamConfigProvider::CIO2);
    if (status != OK) {
        LOGE("Failed to configure input system.");
        return status;
    }

    status = gc->getSensorFrameParams(mMediaCtlHelper.getConfigResults().sensorFrameParams);
    if (status != NO_ERROR) {
        LOGE("Failed to calculate Frame Params, status:%d", status);
        return status;
    }

    status = mPollerThread->init((std::vector<std::shared_ptr<cros::V4L2Device>>&) mConfiguredNodes,
                                 this, POLLPRI | POLLIN | POLLOUT | POLLERR, false);
    if (status != NO_ERROR) {
        LOGE("PollerThread init failed (ret = %d)", status);
        return status;
    }

    if (msg.result)
        *msg.result = mMediaCtlHelper.getConfigResults();

    return status;
}

status_t InputSystem::opened(IPU3NodeNames isysNodeName,
        std::shared_ptr<cros::V4L2VideoNode> videoNode)
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

    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleStart, base::Unretained(this));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

status_t InputSystem::handleStart()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int ret = 0;

    for (size_t i = 0; i < mConfiguredNodes.size(); i++) {
        ret = mConfiguredNodes[i]->Start();
        if (ret < 0) {
            LOGE("STREAMON failed (%s)", mConfiguredNodes[i]->Name().c_str());
            status = UNKNOWN_ERROR;
            MessageStop msg;
            msg.stop = false;
            handleStop(msg);
            break;
        }
    }

    mPollErrorTimes = 0;
    if (status == NO_ERROR)
        mStarted = true;
    return status;
}

status_t InputSystem::stop()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    MessageStop msg;
    msg.stop = true;
    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleStop, base::Unretained(this),
                       base::Passed(std::move(msg)));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

status_t InputSystem::handleStop(MessageStop msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int ret = 0;
    mBufferSeqNbr = 0;

    mPollerThread->flush(true);

    for (size_t i = 0; i < mConfiguredNodes.size(); i++) {
        ret = mConfiguredNodes[i]->Stop();
        if (ret < 0) {
            LOGE("STREAMOFF failed (%s)", mConfiguredNodes[i]->Name().c_str());
            status = UNKNOWN_ERROR;
        }
    }

    // Video nodes will really stop after the buffer pools released
    mStarted = false;

    return status;
}

status_t InputSystem::releaseBufferPools()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleReleaseBufferPools,
                       base::Unretained(this));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

status_t InputSystem::handleReleaseBufferPools()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    int ret = 0;

    // destroyBufferPool() is a private method in V4L2 class and
    // it is combined into stop() method. For example, mmap'ed
    // buffers require unmapping between STREAMOFF and releasing
    // buffer pool. This method allows doing these steps separately.
    for (size_t i = 0; i < mConfiguredNodes.size(); i++) {
        ret = mConfiguredNodes[i]->Stop();
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

    return status;
}

bool InputSystem::isStarted()
{
    LOG1("@%s", __FUNCTION__);

    bool value = false;
    MessageBoolQuery msg;
    msg.value = &value;
    status_t status = NO_ERROR;
    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleIsStarted, base::Unretained(this),
                       base::Passed(std::move(msg)));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return value;
}

status_t InputSystem::handleIsStarted(MessageBoolQuery msg)
{
    LOG1("@%s", __FUNCTION__);
    if(msg.value)
        *msg.value = mStarted;

    return NO_ERROR;
}

status_t InputSystem::putFrame(IPU3NodeNames isysNodeName,
                               cros::V4L2Buffer *buf, int32_t reqId)
{
    LOG2("@%s", __FUNCTION__);

    MessageFrame msg;
    msg.reqId = reqId;
    msg.isysNodeName = isysNodeName;
    msg.buf = buf;

    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handlePutFrame, base::Unretained(this),
                       base::Passed(std::move(msg)));
    mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
    return NO_ERROR;
}

status_t InputSystem::handlePutFrame(MessageFrame msg)
{
    LOG2("@%s", __FUNCTION__);

    std::shared_ptr<IsysRequest> isysReq;
    status_t status = NO_ERROR;
    bool newReq = false;
    IPU3NodeNames isysNodeName = msg.isysNodeName;
    cros::V4L2Buffer *buf = msg.buf;
    const int32_t reqId = msg.reqId;

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
    std::shared_ptr<cros::V4L2VideoNode> videoNode = it->second;
    int ret = videoNode->PutFrame(buf);
    if (ret < 0) {
        LOGE("isys putframe failed for dev: %s", videoNode->Name().c_str());
        return UNKNOWN_ERROR;
    }

    CheckError(isysReq == nullptr, UNKNOWN_ERROR, "@%s: isysReq is nullptr", __FUNCTION__);
    isysReq->configuredNodesForRequest.push_back(videoNode);
    isysReq->numNodesForRequest = isysReq->configuredNodesForRequest.size();
    if (newReq) {
        isysReq->mediaRequestId = 44; // remove
        isysReq->requestId = msg.reqId;
        mPendingIsysRequests.emplace(isysReq->requestId, isysReq);
    }

    return NO_ERROR;
}

status_t InputSystem::grabFrame(IPU3NodeNames isysNodeName, cros::V4L2Buffer *buf)
{
    LOG2("@%s", __FUNCTION__);
    ConfiguredNodesPerName::iterator it =
                            mConfiguredNodesPerName.find(isysNodeName);
    if (it == mConfiguredNodesPerName.end()) {
        LOGE("ISYS node (%d) not found!", isysNodeName);
        return BAD_VALUE;
    }
    std::shared_ptr<cros::V4L2VideoNode> videoNode = it->second;
    int ret = videoNode->GrabFrame(buf);
    if (ret < 0) {
        LOGE("@%s failed", __FUNCTION__);
        return UNKNOWN_ERROR;
    }
    return NO_ERROR;
}

status_t InputSystem::setBufferPool(IPU3NodeNames isysNodeName,
                                    std::vector<cros::V4L2Buffer> &pool,
                                    bool cached)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    MessageBufferPool msg;
    msg.isysNodeName = isysNodeName;
    msg.pool = &pool;
    msg.cached = cached;
    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleSetBufferPool,
                       base::Unretained(this), base::Passed(std::move(msg)));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);

    return status;
}

status_t InputSystem::handleSetBufferPool(MessageBufferPool msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    IPU3NodeNames isysNodeName = msg.isysNodeName;
    std::vector<cros::V4L2Buffer> *pool = msg.pool;
    size_t num_buffers = pool->size();
    bool cached = msg.cached;
    std::shared_ptr<cros::V4L2VideoNode> videoNode = nullptr;

    enum v4l2_memory memType = getDefaultMemoryType(ISYS_NODE_RAW);

    ConfiguredNodesPerName::iterator it =
                            mConfiguredNodesPerName.find(isysNodeName);
    if (it == mConfiguredNodesPerName.end()) {
        LOGE("@%s: ISYS node (%d) not found!", __FUNCTION__, isysNodeName);
        return BAD_VALUE;
    }
    videoNode = it->second;
    pool->clear();
    status = videoNode->SetupBuffers(num_buffers, cached, memType, pool);
    if (status != NO_ERROR) {
        LOGE("Failed setting buffer poll into the device.");
        return status;
    }

    return status;
}

status_t InputSystem::getOutputNodes(ConfiguredNodesPerName **nodes,
                                            int &nodeCount)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    MessageNodes msg;
    msg.nodes = nodes;
    msg.nodeCount = &nodeCount;
    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleGetOutputNodes,
                       base::Unretained(this), base::Passed(std::move(msg)));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

std::shared_ptr<cros::V4L2VideoNode> InputSystem::findOutputNode(IPU3NodeNames isysNodeName)
{
    auto it = mConfiguredNodesPerName.find(isysNodeName);
    if (it == mConfiguredNodesPerName.end()) {
        LOGE("@%s ISYS node (%d) not found!", __FUNCTION__, isysNodeName);
        return nullptr;
    }
    return it->second;
}

status_t InputSystem::handleGetOutputNodes(MessageNodes msg)
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    ConfiguredNodesPerName **nodes = msg.nodes;
    int *nodeCount = msg.nodeCount;

    if (nodes)
        *nodes = &mConfiguredNodesPerName;

    if (nodeCount)
        *nodeCount = mConfiguredNodes.size();

    return status;
}

status_t InputSystem::enqueueMediaRequest(int32_t reqId)
{
    LOG2("@%s, reqId = %d", __FUNCTION__, reqId);
    MessageEnqueueMediaRequest msg;
    msg.requestId = reqId;
    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleEnqueueMediaRequest,
                       base::Unretained(this), base::Passed(std::move(msg)));
    mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
    return NO_ERROR;
}

status_t InputSystem::handleEnqueueMediaRequest(
        MessageEnqueueMediaRequest msg)
{
    LOG2("@%s", __FUNCTION__);
    int32_t reqId = msg.requestId;
    std::shared_ptr<IsysRequest> currentRequest = nullptr;
    /* first checking if existing mediaRequest created */
    auto itRequest = mPendingIsysRequests.find(reqId);
    if (itRequest == mPendingIsysRequests.end()) {
        LOGE("No request pending for reqId %d, BUG!", reqId);
        return UNKNOWN_ERROR;
    }

    currentRequest = mPendingIsysRequests.at(reqId);
    CheckError(currentRequest == nullptr, UNKNOWN_ERROR, "@%s: currentRequest is nullptr", __FUNCTION__);
    mMediaCtl->enqueueMediaRequest(currentRequest->mediaRequestId);
    return OK;
}

status_t InputSystem::capture(int requestId)
{
    LOG2("@%s: request ID: %d", __FUNCTION__, requestId);
    MessageCapture msg;
    msg.requestId = requestId;

    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleCapture,
                       base::Unretained(this), base::Passed(std::move(msg)));
    mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
    return NO_ERROR;
}

status_t InputSystem::handleCapture(MessageCapture msg)
{
    LOG2("@%s", __FUNCTION__);

    mCaptureQueue.push_back(msg.requestId);
    // Start polling if all buffers for the previous request have
    // been received
    if (mRequestDone) {
        pollNextRequest();
        mRequestDone = false;
    }
    return NO_ERROR;
}

status_t InputSystem::flush()
{
    LOG1("@%s", __FUNCTION__);
    status_t status = NO_ERROR;

    // flush the poll messages
    base::Callback<status_t()> closure =
            base::Bind(&InputSystem::handleFlush, base::Unretained(this));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

status_t InputSystem::handleFlush()
{
    LOG1("@%s:", __FUNCTION__);
    mPollerThread->flush(true);
    return NO_ERROR;
}

status_t InputSystem::handleExit()
{
    LOG1("@%s", __FUNCTION__);
    // stop streaming before closing devices
    if (mStarted) {
        MessageStop msg;
        msg.stop = false;
        handleStop(msg);
    }
    mStarted = false;
    return NO_ERROR;
}

status_t InputSystem::notifyPollEvent(PollEventMessage *pollMsg)
{
    LOG2("@%s", __FUNCTION__);

    if (pollMsg == nullptr || pollMsg->data.activeDevices == nullptr)
        return BAD_VALUE;

    // Common thread message fields for any case
    MessagePollEvent msg;
    msg.pollMsgId = pollMsg->id;
    msg.requestId = pollMsg->data.reqId;

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

        msg.activeDevices = new std::shared_ptr<cros::V4L2VideoNode>[numDevices];
        for (int i = 0; i < numDevices; i++) {
            msg.activeDevices[i] = (std::shared_ptr<cros::V4L2VideoNode>&) pollMsg->data.activeDevices->at(i);
        }
        msg.numDevices = numDevices;
        msg.polledDevices = numPolledDevices;

        base::Callback<status_t()> closure =
                base::Bind(&InputSystem::handlePollEvent,
                           base::Unretained(this), base::Passed(std::move(msg)));
        mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);

        if (pollMsg->data.activeDevices->size() != pollMsg->data.polledDevices->size()) {
            LOG2("@%s: %zu inactive nodes for request %d, retry poll", __FUNCTION__,
                                                                      pollMsg->data.inactiveDevices->size(),
                                                                      pollMsg->data.reqId);
            pollMsg->data.polledDevices->clear();
            *pollMsg->data.polledDevices = *pollMsg->data.inactiveDevices; // retry with inactive devices
            return -EAGAIN;
        }
    } else if (pollMsg->id == POLL_EVENT_ID_ERROR) {
        LOGE("device poll failed");
        // For now, set number of device to zero in error case
        msg.numDevices = 0;
        msg.polledDevices = 0;
        base::Callback<status_t()> closure =
                base::Bind(&InputSystem::handlePollEvent,
                           base::Unretained(this), base::Passed(std::move(msg)));
        mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
    }

    return OK;
}

status_t InputSystem::getIsysNodeName(std::shared_ptr<cros::V4L2VideoNode> node, IPU3NodeNames &isysNodeName)
{
    LOG2("@%s", __FUNCTION__);

    for (const auto &configNode : mConfiguredNodesPerName) {
        if (configNode.second.get() == node.get()) {
            isysNodeName = configNode.first;
            return NO_ERROR;
        }
    }

    return BAD_VALUE;
}

status_t InputSystem::handlePollEvent(MessagePollEvent msg)
{
    LOG2("@%s", __FUNCTION__);
    status_t status = NO_ERROR;
    cros::V4L2Buffer outBuf;
    std::shared_ptr<cros::V4L2VideoNode> *activeNodes;
    IPU3NodeNames isysNodeName = IMGU_NODE_NULL;
    uint8_t nodeCount = mCaptureInProgress->numNodesForRequest;
    int activeNodecount = 0;
    int requestId = -999;

    activeNodes = msg.activeDevices;
    activeNodecount = msg.numDevices;
    requestId = msg.requestId;

    LOG2("@%s: received %d / %d buffers for request Id %d", __FUNCTION__,
                                                            activeNodecount,
                                                            nodeCount,
                                                            requestId);

    if (msg.pollMsgId == POLL_EVENT_ID_ERROR) {
        // Notify observer
        IISysObserver::IsysMessage isysMsg;
        isysMsg.id = IISysObserver::ISYS_MESSAGE_ID_ERROR;
        isysMsg.data.error.status = status;
        mObserver->notifyIsysEvent(isysMsg);
        // Poll again if poll error times is less than the threshold.
        if (mPollErrorTimes++ < POLL_REQUEST_TRY_TIMES) {
            status = mPollerThread->pollRequest(mCaptureInProgress->requestId,
                                                IPU3_EVENT_POLL_TIMEOUT,
                                                (std::vector<std::shared_ptr<cros::V4L2Device>>*) &mCaptureInProgress->configuredNodesForRequest);
            return status;
        } else if (mErrCb) {
            // return a error message if poll failed happens 5 times in succession
            mErrCb->deviceError();
            return UNKNOWN_ERROR;
        }
    }

    mPollErrorTimes = 0;
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
                       (std::vector<std::shared_ptr<cros::V4L2Device>>*) &mCaptureInProgress->configuredNodesForRequest);
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
                       (std::vector<std::shared_ptr<cros::V4L2Device>>*) &mCaptureInProgress->configuredNodesForRequest);
            return status;
        }

        // When receiving the first buffer for a request,
        // store the sequence number. All buffers should
        // have the same sequence number.
        if (mBufferSeqNbr == 0) {
            mBufferSeqNbr = outBuf.Sequence();
        } else if (mBufferSeqNbr != outBuf.Sequence()) {
            LOGW("Sequence number mismatch, expecting %d but received %d",
                  mBufferSeqNbr, outBuf.Sequence());
            mBufferSeqNbr = outBuf.Sequence();
        }
        LOG2("input system outBuf.sequence %u", outBuf.Sequence());
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
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2, LOG_TAG);
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
                                        (std::vector<std::shared_ptr<cros::V4L2Device>>*) &mCaptureInProgress->configuredNodesForRequest);

    return status;
}

}
}
