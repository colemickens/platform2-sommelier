/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#define LOG_TAG "ImguUnit"

#include "ImguUnit.h"

#include <vector>

#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "IPU3CapturedStatistics.h"
#include "workers/InputFrameWorker.h"
#include "workers/OutputFrameWorker.h"
#include "workers/StatisticsWorker.h"
#include "IMGUTypes.h"

namespace android {
namespace camera2 {

ImguUnit::ImguUnit(int cameraId,
                   GraphConfigManager &gcm,
                   std::shared_ptr<MediaController> mediaCtl) :
        mState(IMGU_IDLE),
        mCameraId(cameraId),
        mGCM(gcm),
        mThreadRunning(false),
        mMessageQueue("ImguUnitThread", static_cast<int>(MESSAGE_ID_MAX)),
        mMediaCtlHelper(mediaCtl, nullptr, true),
        mPollerThread(new PollerThread("ImguPollerThread")),
        mFirstRequest(true)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mActiveStreams.inputStream = nullptr;

    mMessageThread = std::unique_ptr<MessageThread>(new MessageThread(this, "ImguThread"));
    if (mMessageThread == nullptr) {
        LOGE("Error creating poller thread");
        return;
    }
    mMessageThread->run();

    mRgbsGridBuffPool = std::make_shared<SharedItemPool<ia_aiq_rgbs_grid>>("RgbsGridBuffPool");
    mAfFilterBuffPool = std::make_shared<SharedItemPool<ia_aiq_af_grid>>("AfFilterBuffPool");

    status_t status = allocatePublicStatBuffers(PUBLIC_STATS_POOL_SIZE);
    if (status != NO_ERROR) {
        LOGE("Failed to allocate statistics, status: %d.", status);
        return;
    }
}

ImguUnit::~ImguUnit()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;

    if (mPollerThread) {
        status |= mPollerThread->requestExitAndWait();
        mPollerThread.reset();
    }
    mNodes.clear();

    requestExitAndWait();
    if (mMessageThread != nullptr) {
        mMessageThread.reset();
        mMessageThread = nullptr;
    }

    if (mMessagesUnderwork.size())
        LOGW("There are messages that are not processed %d:", mMessagesUnderwork.size());
    if (mMessagesPending.size())
        LOGW("There are pending messages %d:", mMessagesPending.size());

    mActiveStreams.blobStreams.clear();
    mActiveStreams.rawStreams.clear();
    mActiveStreams.yuvStreams.clear();

    cleanListener();

    /* Delete workers. */
    mPollableWorkers.clear();
    mDeviceWorkers.clear();
    mFirstWorkers.clear();

    freePublicStatBuffers();
    mRgbsGridBuffPool.reset();
    mAfFilterBuffPool.reset();
}

/**
 * allocatePublicStatBuffers
 *
 * This method allocates the memory for the pool of 3A statistics.
 * The pools are also initialized here.
 *
 * These statistics are the public stats that will be sent to the 3A algorithms.
 *
 * Please do not confuse with the buffers allocated by the driver to get
 * the HW generated statistics. Those are allocated at createStatsBufferPool()
 *
 * The symmetric method to this is freePublicStatBuffers
 * The buffers allocated here are the output of the conversion process
 * from HW generated statistics. This processing is done using the
 * parameter adaptor class.
 *
 * \param[in] numBufs: number of buffers to initialize
 *
 * \return OK everything went fine
 * \return NO_MEMORY failed to allocate
 */
status_t ImguUnit::allocatePublicStatBuffers(int numBufs)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = OK;
    int maxGridSize;
    int allocated = 0;
    std::shared_ptr<ia_aiq_rgbs_grid> rgbsGrid = nullptr;
    std::shared_ptr<ia_aiq_af_grid> afGrid = nullptr;

    maxGridSize = IPU3_MAX_STATISTICS_WIDTH * IPU3_MAX_STATISTICS_HEIGHT;
    status = mAfFilterBuffPool->init(numBufs);
    status |= mRgbsGridBuffPool->init(numBufs);
    if (status != OK) {
        LOGE("Failed to initialize 3A statistics pools");
        freePublicStatBuffers();
        return NO_MEMORY;
    }

    for (allocated = 0; allocated < numBufs; allocated++) {
        status = mAfFilterBuffPool->acquireItem(afGrid);
        status |= mRgbsGridBuffPool->acquireItem(rgbsGrid);

        if (status != OK ||
            afGrid.get() == nullptr ||
            rgbsGrid.get() == nullptr) {
            LOGE("Failed to acquire 3A statistics memory from pools");
            freePublicStatBuffers();
            return NO_MEMORY;
        }

        rgbsGrid->blocks_ptr = new rgbs_grid_block[maxGridSize];
        rgbsGrid->grid_height = 0;
        rgbsGrid->grid_width = 0;

        afGrid->filter_response_1 = new int[maxGridSize];
        afGrid->filter_response_2 = new int[maxGridSize];
        afGrid->block_height = 0;
        afGrid->block_width = 0;
        afGrid->grid_height = 0;
        afGrid->grid_width = 0;

    }
    return NO_ERROR;
}

void ImguUnit::freePublicStatBuffers()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    std::shared_ptr<ia_aiq_rgbs_grid> rgbsGrid = nullptr;
    std::shared_ptr<ia_aiq_af_grid> afGrid = nullptr;
    status_t status = OK;
    if (!mAfFilterBuffPool->isFull() ||
        !mRgbsGridBuffPool->isFull()) {
        LOGE("We are leaking stats- AF:%s RGBS:%s",
                mAfFilterBuffPool->isFull()? "NO" : "YES",
                mRgbsGridBuffPool->isFull()? "NO" : "YES");
    }
    size_t availableItems = mAfFilterBuffPool->availableItems();
    for (size_t i = 0; i < availableItems; i++) {
        status = mAfFilterBuffPool->acquireItem(afGrid);
        if (status == OK && afGrid.get() != nullptr) {
            DELETE_ARRAY_AND_NULLIFY(afGrid->filter_response_1);
            DELETE_ARRAY_AND_NULLIFY(afGrid->filter_response_2);
        } else {
            LOGE("Could not acquire filter response [%d] for deletion - leak?", i);
        }
    }
    availableItems = mRgbsGridBuffPool->availableItems();
    for (size_t i = 0; i < availableItems; i++) {
        status = mRgbsGridBuffPool->acquireItem(rgbsGrid);
        if (status == OK && rgbsGrid.get() != nullptr) {
            DELETE_ARRAY_AND_NULLIFY(rgbsGrid->blocks_ptr);
        } else {
            LOGE("Could not acquire RGBS grid [%d] for deletion - leak?", i);
        }
    }
}

status_t
ImguUnit::configStreams(std::vector<camera3_stream_t*> &activeStreams)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    std::shared_ptr<GraphConfig> graphConfig = mGCM.getBaseGraphConfig();

    mActiveStreams.blobStreams.clear();
    mActiveStreams.rawStreams.clear();
    mActiveStreams.yuvStreams.clear();
    mActiveStreams.inputStream = nullptr;
    mFirstRequest = true;

    for (unsigned int i = 0; i < activeStreams.size(); ++i) {
        if (activeStreams.at(i)->stream_type == CAMERA3_STREAM_INPUT) {
            mActiveStreams.inputStream = activeStreams.at(i);
            continue;
        }

        switch (activeStreams.at(i)->format) {
        case HAL_PIXEL_FORMAT_BLOB:
             mActiveStreams.blobStreams.push_back(activeStreams.at(i));
             graphConfig->setPipeType(GraphConfig::PIPE_STILL);
             break;
        case HAL_PIXEL_FORMAT_RAW16:
             mActiveStreams.rawStreams.push_back(activeStreams.at(i));
             break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
             mActiveStreams.yuvStreams.push_back(activeStreams.at(i));
             break;
        default:
            LOGW("Unsupported stream format %d",
                 activeStreams.at(i)->format);
            break;
        }
    }

    status_t status = createProcessingTasks(graphConfig);
    if (status != NO_ERROR) {
       LOGE("Processing tasks creation failed (ret = %d)", status);
       return UNKNOWN_ERROR;
    }

    status = mPollerThread->init(mNodes,
                                 this, POLLPRI | POLLIN | POLLOUT | POLLERR, false);
    if (status != NO_ERROR) {
       LOGE("PollerThread init failed (ret = %d)", status);
       return UNKNOWN_ERROR;
    }

    return OK;
}

status_t ImguUnit::mapStreamWithDeviceNode()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    int blobNum = mActiveStreams.blobStreams.size();
    int yuvNum = mActiveStreams.yuvStreams.size();
    LOG2("@%s, blobNum:%d, yuvNum:%d", __FUNCTION__, blobNum, yuvNum);

    IPU3NodeNames videoNode = IMGU_NODE_VIDEO;
    IPU3NodeNames previewNode = IMGU_NODE_PREVIEW;

    mStreamNodeMapping.clear();

    if (blobNum == 1) {
        if (yuvNum == 0) { // 1 JPEG stream only
            mStreamNodeMapping[videoNode] = mActiveStreams.blobStreams[0];
            mStreamNodeMapping[previewNode] = nullptr;
        } else if (yuvNum == 1) { // 1 JPEG stream and 1 YUV stream
            camera3_stream_t* sYuv = mActiveStreams.yuvStreams[0];
            camera3_stream_t* sBlob = mActiveStreams.blobStreams[0];
            if (sBlob->width >= sYuv->width) {
                mStreamNodeMapping[videoNode] = mActiveStreams.blobStreams[0];
                mStreamNodeMapping[previewNode] = mActiveStreams.yuvStreams[0];
            } else {
                mStreamNodeMapping[videoNode] = mActiveStreams.yuvStreams[0];
                mStreamNodeMapping[previewNode] = mActiveStreams.blobStreams[0];
            }
        } else {
            LOGE("@%s, line:%d, ERROR, blobNum:%d, yuvNum:%d", __FUNCTION__, __LINE__, blobNum, yuvNum);
            return UNKNOWN_ERROR;
        }

        return OK;
    } else if (blobNum > 1) {
        LOGE("@%s, line:%d, ERROR, blobNum:%d, yuvNum:%d", __FUNCTION__, __LINE__, blobNum, yuvNum);
        return UNKNOWN_ERROR;
    } else {
        LOG2("@%s, no blob", __FUNCTION__);
    }

    if (yuvNum == 1) { // 1 YUV stream only
        mStreamNodeMapping[videoNode] = mActiveStreams.yuvStreams[0];
        mStreamNodeMapping[previewNode] = nullptr;
    } else if (yuvNum == 2) { // 2 YUV streams
        int maxWidth = 0;
        int idx = 0;
        for (int i = 0; i < yuvNum; i++) {
            camera3_stream_t* s = mActiveStreams.yuvStreams[i];
            if (s->width > maxWidth) {
                maxWidth = s->width;
                idx = i;
            }
        }
        mStreamNodeMapping[videoNode] = mActiveStreams.yuvStreams[idx];
        mStreamNodeMapping[previewNode] = mActiveStreams.yuvStreams[1 - idx];
    } else {
        LOGE("@%s, ERROR, the current yuv stream number is:%d", __FUNCTION__, yuvNum);
        return UNKNOWN_ERROR;
    }

    return OK;
}

/**
 * Create the processing tasks and listening tasks.
 * Processing tasks are:
 *  - video task (wraps video pipeline)
 *  - capture task (wraps still capture)
 *  - raw bypass (not done yet)
 *
 * \param[in] activeStreams StreamConfig struct filled during configStreams
 * \param[in] graphConfig Configuration of the base graph
 */
status_t
ImguUnit::createProcessingTasks(std::shared_ptr<GraphConfig> graphConfig)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = OK;

    if (CC_UNLIKELY(graphConfig.get() == nullptr)) {
        LOGE("ERROR: Graph config is nullptr");
        return UNKNOWN_ERROR;
    }

    mNodes.clear();
    // Open and configure imgu video nodes
    status = mMediaCtlHelper.configure(mGCM, IStreamConfigProvider::IMGU);
    if (status != OK)
        return UNKNOWN_ERROR;

    mConfiguredNodesPerName = mMediaCtlHelper.getConfiguredNodesPerName();
    if (mConfiguredNodesPerName.size() == 0) {
        LOGD("No nodes present");
        return UNKNOWN_ERROR;
    }

    if (mapStreamWithDeviceNode() != OK)
        return UNKNOWN_ERROR;

    for (const auto &it : mConfiguredNodesPerName) {
        if (it.first == IMGU_NODE_INPUT) {
            std::shared_ptr<FrameWorker> tmp = nullptr;
            tmp = std::make_shared<InputFrameWorker>(it.second, mCameraId);
            mDeviceWorkers.push_back(tmp); // Input frame;
            mPollableWorkers.push_back(tmp);
            mNodes.push_back(tmp->getNode()); // Nodes are added for pollthread init
            mFirstWorkers.push_back(tmp);
        } else if (it.first == IMGU_NODE_STAT) {
            std::shared_ptr<StatisticsWorker> worker =
                std::make_shared<StatisticsWorker>(it.second, mCameraId, mAfFilterBuffPool, mRgbsGridBuffPool);
            mListenerDeviceWorkers.push_back(worker.get());
            mDeviceWorkers.push_back(worker);
            mPollableWorkers.push_back(worker);
            mNodes.push_back(worker->getNode());
        } else if (it.first == IMGU_NODE_PARAM) {
            std::shared_ptr<ParameterWorker> tmp = nullptr;
            tmp = std::make_shared<ParameterWorker>(it.second, mCameraId);
            mFirstWorkers.push_back(tmp);
            mDeviceWorkers.push_back(tmp); // parameters
        } else if (it.first == IMGU_NODE_STILL || it.first == IMGU_NODE_PREVIEW || it.first == IMGU_NODE_VIDEO) {
            std::shared_ptr<FrameWorker> tmp = nullptr;
            tmp = std::make_shared<OutputFrameWorker>(it.second, mCameraId, mStreamNodeMapping[it.first], it.first);
            mDeviceWorkers.push_back(tmp);
            mPollableWorkers.push_back(tmp);
            mNodes.push_back(tmp->getNode());
        } else if (it.first == IMGU_NODE_RAW) {
            LOGW("Not implemented"); // raw
            continue;
        } else {
            LOGE("Unknown NodeName: %d", it.first);
            return UNKNOWN_ERROR;
        }
    }

    status_t ret = OK;
    for (const auto &it : mDeviceWorkers) {
        ret = (*it).configure(graphConfig);

        if (ret != OK) {
            LOGE("Failed to configure workers.");
            return ret;
        }
        ret = (*it).startWorker();
        if (ret != OK) {
            LOGE("Failed to start workers.");
            return ret;
        }
    }

    std::vector<ICaptureEventSource*>::iterator it = mListenerDeviceWorkers.begin();
    for (;it != mListenerDeviceWorkers.end(); ++it) {
        for (const auto &listener : mListeners) {
            (*it)->attachListener(listener);
        }
    }

    return OK;
}

void
ImguUnit::cleanListener()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    // clean all the listening tasks
    std::shared_ptr<ITaskEventListener> lTask = nullptr;
    for (unsigned int i = 0; i < mListeningTasks.size(); i++) {
        lTask = mListeningTasks.at(i);
        if (lTask.get() == nullptr)
            LOGE("Listening task null - BUG.");
        else
            lTask->cleanListeners();
    }

    mListeningTasks.clear();
}

status_t ImguUnit::attachListener(ICaptureEventListener *aListener)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    mListeners.push_back(aListener);

    return OK;
}

status_t
ImguUnit::completeRequest(std::shared_ptr<ProcUnitSettings> &processingSettings,
                          ICaptureEventListener::CaptureBuffers &captureBufs,
                          bool updateMeta)
{
        HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
        Camera3Request *request = processingSettings->request;
        if (CC_UNLIKELY(request == nullptr)) {
            LOGE("ProcUnit: nullptr request - BUG");
            return UNKNOWN_ERROR;
        }
        const std::vector<camera3_stream_buffer> *outBufs = request->getOutputBuffers();
        const std::vector<camera3_stream_buffer> *inBufs = request->getInputBuffers();
        int reqId = request->getId();

        LOG2("@%s: Req id %d,  Num outbufs %d Num inbufs %d",
             __FUNCTION__, reqId, outBufs ? outBufs->size() : 0, inBufs ? inBufs->size() : 0);

        if (captureBufs.rawNonScaledBuffer.get() != nullptr) {
            LOG2("Using Non Scaled Buffer %p for req id %d",
                 reinterpret_cast<void*>(captureBufs.rawNonScaledBuffer->buf->data()), reqId);
        }

        ProcTaskMsg procMsg;
        procMsg.rawNonScaledBuffer = captureBufs.rawNonScaledBuffer;
        procMsg.reqId = reqId;
        procMsg.processingSettings = processingSettings;

        MessageCallbackMetadata cbMetadataMsg;
        cbMetadataMsg.updateMeta = updateMeta;
        cbMetadataMsg.request = request;

        DeviceMessage msg;
        msg.id = MESSAGE_COMPLETE_REQ;
        msg.pMsg = procMsg;
        msg.cbMetadataMsg = cbMetadataMsg;
        mMessageQueue.send(&msg);

        return NO_ERROR;
}

status_t
ImguUnit::handleMessageCompleteReq(DeviceMessage &msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = NO_ERROR;

    Camera3Request *request = msg.cbMetadataMsg.request;
    if (request == nullptr) {
        LOGE("Request is nullptr");
        return BAD_VALUE;
    }
    LOG2("order %s:enqueue for Req id %d, ", __FUNCTION__, request->getId());
    std::shared_ptr<DeviceMessage> tmp = std::make_shared<DeviceMessage>(msg);
    mMessagesPending.push_back(tmp);

    return processNextRequest();
}

status_t ImguUnit::processNextRequest()
{
    status_t status = NO_ERROR;
    std::shared_ptr<DeviceMessage> msg = nullptr;

    LOG2("%s: pending size %zu, state %d", __FUNCTION__, mMessagesPending.size(), mState);
    if (mMessagesPending.empty())
        return NO_ERROR;

    if (mState == IMGU_RUNNING) {
        LOGD("IMGU busy - message put into a waiting queue");
        return NO_ERROR;
    }

    msg = mMessagesPending[0];
    mMessagesPending.erase(mMessagesPending.begin());

    // update and return metadata firstly
    Camera3Request *request = msg->cbMetadataMsg.request;
    if (request == nullptr) {
        LOGE("Request is nullptr");
        return BAD_VALUE;
    }
    LOG2("@%s:handleExecuteReq for Req id %d, ", __FUNCTION__, request->getId());

    // Pass settings to the listening tasks *before* sending metadata
    // up to framework. Some tasks might need e.g. the result data.
    std::shared_ptr<ITaskEventListener> lTask = nullptr;
    for (unsigned int i = 0; i < mListeningTasks.size(); i++) {
        lTask = mListeningTasks.at(i);
        if (lTask.get() == nullptr) {
            LOGE("Listening task null - BUG.");
            return UNKNOWN_ERROR;
        }
        status |= lTask->settings(msg->pMsg);
    }

    if (msg->cbMetadataMsg.updateMeta) {
        updateProcUnitResults(*request, msg->pMsg.processingSettings);
        //return the metadata
        request->mCallback->metadataDone(request, CONTROL_UNIT_PARTIAL_RESULT);
    }

    mMessagesUnderwork.push_back(msg);

    if (mFirstRequest) {
        status = kickstart();
        if (status != OK) {
            return status;
        }
    }

    std::vector<std::shared_ptr<IDeviceWorker>>::iterator it = mDeviceWorkers.begin();
    for (;it != mDeviceWorkers.end(); ++it) {
        status = (*it)->prepareRun(msg);
        if (status != OK) {
            return status;
        }
    }

    mNodes.clear();
    std::vector<std::shared_ptr<FrameWorker>>::iterator pollDevice = mPollableWorkers.begin();
    for (;pollDevice != mPollableWorkers.end(); ++pollDevice) {
        bool needsPolling = (*pollDevice)->needPolling();
        if (needsPolling) {
            mNodes.push_back((*pollDevice)->getNode());
        }
    }

    status = mPollerThread->pollRequest(request->getId(),
                                        500000,
                                        &mNodes);

    if (status != OK)
        return status;

    mState = IMGU_RUNNING;

    return status;
}

status_t
ImguUnit::kickstart()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = OK;
    std::vector<std::shared_ptr<IDeviceWorker>>::iterator firstit = mFirstWorkers.begin();
    firstit = mFirstWorkers.begin();
    for (;firstit != mFirstWorkers.end(); ++firstit) {
        status |= (*firstit)->prepareRun(mMessagesUnderwork[0]);
    }
    if (status != OK) {
        return status;
    }

    firstit = mFirstWorkers.begin();
    for (;firstit != mFirstWorkers.end(); ++firstit) {
        status |= (*firstit)->run();
    }
    if (status != OK) {
        return status;
    }

    firstit = mFirstWorkers.begin();
    for (;firstit != mFirstWorkers.end(); ++firstit) {
        status |= (*firstit)->postRun();
    }
    if (status != OK) {
        return status;
    }

    mFirstRequest = false;
    return status;
}

status_t
ImguUnit::updateProcUnitResults(Camera3Request &request,
                                std::shared_ptr<ProcUnitSettings> settings)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    status_t status = NO_ERROR;
    CameraMetadata *ctrlUnitResult = nullptr;

    ctrlUnitResult = request.getPartialResultBuffer(CONTROL_UNIT_PARTIAL_RESULT);

    if (ctrlUnitResult == nullptr) {
        LOGE("Failed to retrieve Metadata buffer for reqId = %d find the bug!",
                request.getId());
        return UNKNOWN_ERROR;
    }

    // update DVS metadata
    updateDVSMetadata(*ctrlUnitResult, settings);

    // update misc metadata (split if need be)
    updateMiscMetadata(*ctrlUnitResult, settings);
    return status;
}

/**
 * Start the processing task for each input buffer.
 * Each of the input buffers has an associated terminal id. This is the
 * destination terminal id. This terminal id is the input terminal for one
 * or the execute tasks we have.
 *
 * Check the map that links the input terminals of the pipelines to the
 * tasks that wrap them to decide which tasks need to be executed.
 */
status_t
ImguUnit::startProcessing()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    status_t status = OK;
    std::vector<std::shared_ptr<IDeviceWorker>>::iterator it = mDeviceWorkers.begin();
    for (;it != mDeviceWorkers.end(); ++it) {
        status |= (*it)->run();
    }

    it = mDeviceWorkers.begin();
    for (;it != mDeviceWorkers.end(); ++it) {
        status |= (*it)->postRun();
    }

    mMessagesUnderwork.erase(mMessagesUnderwork.begin());

    mState = IMGU_IDLE;

    return status;
}

status_t ImguUnit::notifyPollEvent(PollEventMessage *pollMsg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    if (pollMsg == nullptr || pollMsg->data.activeDevices == nullptr)
        return BAD_VALUE;

    // Common thread message fields for any case
    DeviceMessage msg;
    msg.id = MESSAGE_ID_POLL;
    msg.pollEvent.pollMsgId = pollMsg->id;
    msg.pollEvent.requestId = pollMsg->data.reqId;

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

        msg.pollEvent.activeDevices = new std::shared_ptr<V4L2VideoNode>[numDevices];
        for (int i = 0; i < numDevices; i++) {
            msg.pollEvent.activeDevices[i] = (std::shared_ptr<V4L2VideoNode>&) pollMsg->data.activeDevices->at(i);
        }
        msg.pollEvent.numDevices = numDevices;
        msg.pollEvent.polledDevices = numPolledDevices;

        if (pollMsg->data.activeDevices->size() != pollMsg->data.polledDevices->size()) {
            LOG2("@%s: %d inactive nodes for request %d, retry poll", __FUNCTION__,
                                                                      pollMsg->data.inactiveDevices->size(),
                                                                      pollMsg->data.reqId);
            pollMsg->data.polledDevices->clear();
            *pollMsg->data.polledDevices = *pollMsg->data.inactiveDevices; // retry with inactive devices

            delete [] msg.pollEvent.activeDevices;

            return -EAGAIN;
        }

        mMessageQueue.send(&msg);

    } else if (pollMsg->id == POLL_EVENT_ID_ERROR) {
        LOGE("Device poll failed");
        // For now, set number of device to zero in error case
        msg.pollEvent.numDevices = 0;
        msg.pollEvent.polledDevices = 0;
        mMessageQueue.send(&msg);
    } else {
        LOGW("unknown poll event id (%d)", pollMsg->id);
    }

    return OK;
}

/**
 * update misc metadata
 * metadata which somewhat belongs to the PU's turf
 *
 * \param[IN] procUnitResults Metadata copy to update.
 * \param[IN] settings holds input params for processing unit.
 *
 */
void
ImguUnit::updateMiscMetadata(CameraMetadata &procUnitResults,
                             std::shared_ptr<const ProcUnitSettings> settings) const
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    if (settings.get() == nullptr) {
        LOGE("null settings for Metadata update");
        return;
    }

    //# ANDROID_METADATA_Dynamic android.control.effectMode done
    procUnitResults.update(ANDROID_CONTROL_EFFECT_MODE,
                           &settings->captureSettings->ispControls.effect, 1);
    //# ANDROID_METADATA_Dynamic android.noiseReduction.mode done
    procUnitResults.update(ANDROID_NOISE_REDUCTION_MODE,
                           &settings->captureSettings->ispControls.nr.mode, 1);
    //# ANDROID_METADATA_Dynamic android.edge.mode done
    procUnitResults.update(ANDROID_EDGE_MODE,
                           &settings->captureSettings->ispControls.ee.mode, 1);
}

/**
 * update the DVS metadata
 * only copying from settings to dynamic
 *
 * \param[IN] procUnitResults Metadata copy to update.
 * \param[IN] settings ProcUnitSettings
 *
 */
void
ImguUnit::updateDVSMetadata(CameraMetadata &procUnitResults,
                            std::shared_ptr<const ProcUnitSettings> settings) const
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    if (settings.get() == nullptr) {
        LOGE("null settings in UDVSMetadata");
        return;
    }
    //# ANDROID_METADATA_Dynamic android.control.videoStabilizationMode copied
    procUnitResults.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
                           &settings->captureSettings->videoStabilizationMode,
                           1);
    //# ANDROID_METADATA_Dynamic android.lens.opticalStabilizationMode copied
    procUnitResults.update(ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
                           &settings->captureSettings->opticalStabilizationMode,
                           1);
}

status_t ImguUnit::handleMessagePoll(DeviceMessage msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    delete [] msg.pollEvent.activeDevices;
    msg.pollEvent.activeDevices = nullptr;

    status_t status = startProcessing();
    if (status == NO_ERROR)
        status = processNextRequest();

    return status;
}

void
ImguUnit::messageThreadLoop(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    mThreadRunning = true;
    while (mThreadRunning) {
        status_t status = NO_ERROR;

        DeviceMessage msg;
        mMessageQueue.receive(&msg);

        PERFORMANCE_HAL_ATRACE_PARAM1("msg", msg.id);
        LOG2("@%s, receive message id:%d", __FUNCTION__, msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;
        case MESSAGE_COMPLETE_REQ:
            status = handleMessageCompleteReq(msg);
            break;
        case MESSAGE_ID_POLL:
            status = handleMessagePoll(msg);
            break;
        case MESSAGE_ID_FLUSH:
            status = handleMessageFlush();
            break;
        default:
            LOGE("ERROR Unknown message %d in thread loop", msg.id);
            status = BAD_VALUE;
            break;
        }
        if (status != NO_ERROR)
            LOGE("error %d in handling message: %d",
                 status, static_cast<int>(msg.id));
        LOG2("@%s, finish message id:%d", __FUNCTION__, msg.id);
        mMessageQueue.reply(msg.id, status);
    }
    LOG2("%s: Exit", __FUNCTION__);
}

status_t
ImguUnit::handleMessageExit(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    mThreadRunning = false;
    return NO_ERROR;
}

status_t
ImguUnit::requestExitAndWait(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    DeviceMessage msg;
    msg.id = MESSAGE_ID_EXIT;
    status_t status = mMessageQueue.send(&msg, MESSAGE_ID_EXIT);
    status |= mMessageThread->requestExitAndWait();
    return status;
}

status_t
ImguUnit::flush(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    DeviceMessage msg;
    msg.id = MESSAGE_ID_FLUSH;

    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t
ImguUnit::handleMessageFlush(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    mPollerThread->flush(true);

    mPollableWorkers.clear();
    mDeviceWorkers.clear();
    mFirstWorkers.clear();
    mListenerDeviceWorkers.clear();

    return NO_ERROR;
}
} /* namespace camera2 */
} /* namespace android */
