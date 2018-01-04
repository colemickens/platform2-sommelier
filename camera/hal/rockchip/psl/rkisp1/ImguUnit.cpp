/*
 * Copyright (C) 2016-2017 Intel Corporation.
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#include <vector>
#include <algorithm>
#include "ImguUnit.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "workers/OutputFrameWorker.h"
#include "workers/StatisticsWorker.h"
#include "CameraMetadataHelper.h"


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
        mCurPipeConfig(nullptr),
        mMediaCtlHelper(mediaCtl, nullptr, true),
        mPollerThread(new PollerThread("ImguPollerThread")),
        mFlushing(false),
        mFirstRequest(true),
        mNeedRestartPoll(true),
        mTakingPicture(false)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mActiveStreams.inputStream = nullptr;

    mMessageThread = std::unique_ptr<MessageThread>(new MessageThread(this, "ImguThread"));
    if (mMessageThread == nullptr) {
        LOGE("Error creating poller thread");
        return;
    }
    mMessageThread->run();
    mPollerThreadMeta.reset(new PollerThread("ImguPollerThreadMeta"));
}

ImguUnit::~ImguUnit()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = NO_ERROR;

    if (mPollerThread) {
        status |= mPollerThread->requestExitAndWait();
        mPollerThread.reset();
    }
    if (mPollerThreadMeta) {
        status |= mPollerThreadMeta->requestExitAndWait();
        mPollerThreadMeta.reset();
    }

    requestExitAndWait();
    if (mMessageThread != nullptr) {
        mMessageThread.reset();
        mMessageThread = nullptr;
    }

    if (mMessagesUnderwork.size())
        LOGW("There are messages that are not processed %zu:", mMessagesUnderwork.size());
    if (mMessagesPending.size())
        LOGW("There are pending messages %zu:", mMessagesPending.size());

    mActiveStreams.blobStreams.clear();
    mActiveStreams.rawStreams.clear();
    mActiveStreams.yuvStreams.clear();

    cleanListener();
    clearWorkers();
}

void ImguUnit::clearWorkers()
{
    for (size_t i = 0; i < PIPE_NUM; i++) {
        PipeConfiguration* config = &(mPipeConfigs[i]);
        config->deviceWorkers.clear();
        config->pollableWorkers.clear();
        config->nodes.clear();
    }
    mMetaConfig.deviceWorkers.clear();
    mMetaConfig.pollableWorkers.clear();
    mMetaConfig.nodes.clear();
    mFirstWorkers.clear();
    mListenerDeviceWorkers.clear();
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
    mNeedRestartPoll = true;
    mCurPipeConfig = nullptr;
    mTakingPicture = false;
    mFlushing = false;

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
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
             mActiveStreams.yuvStreams.push_back(activeStreams.at(i));
             break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
             // Always put IMPL stream on the begin for mapping, in the
             // 3 stream case, IMPL is prefered to use for preview
             mActiveStreams.yuvStreams.insert(mActiveStreams.yuvStreams.begin(), activeStreams.at(i));
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

    status = mPollerThread->init(mCurPipeConfig->nodes,
                                 this, POLLPRI | POLLIN | POLLOUT | POLLERR, false);
    status |= mPollerThreadMeta->init(mMetaConfig.nodes,
                                 this, POLLPRI | POLLIN | POLLOUT | POLLERR, false);
    if (status != NO_ERROR) {
       LOGE("PollerThread init failed (ret = %d)", status);
       return UNKNOWN_ERROR;
    }

    return OK;
}

#define streamSizeGT(s1, s2) (((s1)->width * (s1)->height) > ((s2)->width * (s2)->height))
#define streamSizeEQ(s1, s2) (((s1)->width * (s1)->height) == ((s2)->width * (s2)->height))
#define streamSizeGE(s1, s2) (((s1)->width * (s1)->height) >= ((s2)->width * (s2)->height))

status_t ImguUnit::mapStreamWithDeviceNode()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    int blobNum = mActiveStreams.blobStreams.size();
    int yuvNum = mActiveStreams.yuvStreams.size();
    int streamNum = blobNum + yuvNum;

    if (blobNum > 1) {
        LOGE("Don't support blobNum %d", blobNum);
        return BAD_VALUE;
    }

    mStreamNodeMapping.clear();
    mStreamListenerMapping.clear();

    std::vector<camera3_stream_t *> availableStreams = mActiveStreams.yuvStreams;
    if (blobNum) {
        availableStreams.insert(availableStreams.begin(), mActiveStreams.blobStreams[0]);
    }

    LOG1("@%s, %d streams, blobNum:%d, yuvNum:%d", __FUNCTION__, streamNum, blobNum, yuvNum);

    int videoIdx = -1;
    int previewIdx = -1;
    int listenerIdx = -1;
    bool isVideoSnapshot = false;
    NodeTypes listenToNode = IMGU_NODE_NULL;

    if (streamNum == 1) {
        // Force use video, rk use the IMGU_NODE_VIDEO firstly.
        //if second stream is needed, then IMGU_NODE_VF_PREVIEW will be usde.
        //and rk has no IMGU_NODE_PV_PREVIEW.
        videoIdx = 0;
    } else if (streamNum == 2) {
        videoIdx = (streamSizeGE(availableStreams[0], availableStreams[1])) ? 0 : 1;
        previewIdx = videoIdx ? 0 : 1;
    } else if (yuvNum == 2 && blobNum == 1) {
        // Check if it is video snapshot case: jpeg size = yuv size
        // Otherwise it is still capture case, same to GraphConfigManager::mapStreamToKey.
        if (streamSizeEQ(availableStreams[0], availableStreams[1])
            || streamSizeEQ(availableStreams[0], availableStreams[2])) {
            videoIdx = (streamSizeGE(availableStreams[1], availableStreams[2])) ? 1 : 2; // For video stream
            previewIdx = (videoIdx == 1) ? 2 : 1; // For preview stream
            listenerIdx = 0; // For jpeg stream
            listenToNode = IMGU_NODE_VIDEO;
            isVideoSnapshot = true;
        } else {
            previewIdx = (streamSizeGT(availableStreams[1], availableStreams[2])) ? 1
                       : (streamSizeGT(availableStreams[2], availableStreams[1])) ? 2
                       : (availableStreams[1]->usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) ? 2
                       : 1; // For preview stream

            listenerIdx = (previewIdx == 1) ? 2 : 1; // For preview callback stream
            if (streamSizeGT(availableStreams[0], availableStreams[previewIdx])) {
                videoIdx = 0; // For JPEG stream
                listenToNode = IMGU_NODE_VF_PREVIEW;
            } else {
                videoIdx = previewIdx;
                previewIdx = 0; // For JPEG stream
                listenToNode = IMGU_NODE_VIDEO;
            }
        }
    } else {
        LOGE("@%s, ERROR, blobNum:%d, yuvNum:%d", __FUNCTION__, blobNum, yuvNum);
        return UNKNOWN_ERROR;
    }

    if (previewIdx >= 0) {
        mStreamNodeMapping[IMGU_NODE_VF_PREVIEW] = availableStreams[previewIdx];
        mStreamNodeMapping[IMGU_NODE_PV_PREVIEW] = mStreamNodeMapping[IMGU_NODE_VF_PREVIEW];
        LOG1("@%s, %d stream %p size preview: %dx%d, format %s", __FUNCTION__,
             previewIdx, availableStreams[previewIdx],
             availableStreams[previewIdx]->width, availableStreams[previewIdx]->height,
             METAID2STR(android_scaler_availableFormats_values,
                        availableStreams[previewIdx]->format));
    }

    if (videoIdx >= 0) {
        mStreamNodeMapping[IMGU_NODE_VIDEO] = availableStreams[videoIdx];
        LOG1("@%s, %d stream %p size video: %dx%d, format %s", __FUNCTION__,
             videoIdx, availableStreams[videoIdx],
             availableStreams[videoIdx]->width, availableStreams[videoIdx]->height,
             METAID2STR(android_scaler_availableFormats_values,
                        availableStreams[videoIdx]->format));
    }

    if (listenerIdx >= 0) {
        mStreamListenerMapping[availableStreams[listenerIdx]] = listenToNode;
        LOG1("@%s (%dx%d 0x%x), %p listen to 0x%x", __FUNCTION__,
             availableStreams[listenerIdx]->width, availableStreams[listenerIdx]->height,
             availableStreams[listenerIdx]->format, availableStreams[listenerIdx], listenToNode);
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

    clearWorkers();
    // Open and configure imgu video nodes

    // rk only has video config, set it as default,zyc
    mCurPipeConfig = &mPipeConfigs[PIPE_VIDEO_INDEX];

    status = mMediaCtlHelper.configure(mGCM, IStreamConfigProvider::CIO2);
    if (status != OK) {
        LOGE("Failed to configure input system.");
        return status;
    }

    status = mMediaCtlHelper.configure(mGCM, IStreamConfigProvider::IMGU_COMMON);
    if (status != OK)
        return UNKNOWN_ERROR;
    if (mGCM.getMediaCtlConfig(IStreamConfigProvider::IMGU_STILL)) {
        status = mMediaCtlHelper.configurePipe(mGCM, IStreamConfigProvider::IMGU_STILL, true);
        if (status != OK)
            return UNKNOWN_ERROR;
        mCurPipeConfig = &mPipeConfigs[PIPE_STILL_INDEX];
    }
    // Set video pipe by default
    if (mGCM.getMediaCtlConfig(IStreamConfigProvider::IMGU_VIDEO)) {
        status = mMediaCtlHelper.configurePipe(mGCM, IStreamConfigProvider::IMGU_VIDEO, true);
        if (status != OK)
            return UNKNOWN_ERROR;
        mCurPipeConfig = &mPipeConfigs[PIPE_VIDEO_INDEX];
    }

    mConfiguredNodesPerName = mMediaCtlHelper.getConfiguredNodesPerName();
    if (mConfiguredNodesPerName.size() == 0) {
        LOGD("No nodes present");
        return UNKNOWN_ERROR;
    }

    if (mapStreamWithDeviceNode() != OK)
        return UNKNOWN_ERROR;

    PipeConfiguration* videoConfig = &(mPipeConfigs[PIPE_VIDEO_INDEX]);
    PipeConfiguration* stillConfig = &(mPipeConfigs[PIPE_STILL_INDEX]);

    std::shared_ptr<OutputFrameWorker> vfWorker = nullptr;
    std::shared_ptr<OutputFrameWorker> pvWorker = nullptr;
    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    camera_metadata_ro_entry entry;
    CLEAR(entry);
    if (meta)
        entry = MetadataHelper::getMetadataEntry(
            meta, ANDROID_REQUEST_PIPELINE_MAX_DEPTH);
    size_t pipelineDepth = entry.count == 1 ? entry.data.u8[0] : 1;
    for (const auto &it : mConfiguredNodesPerName) {
        std::shared_ptr<FrameWorker> worker = nullptr;
        if (it.first == IMGU_NODE_STAT) {
            std::shared_ptr<StatisticsWorker> statWorker =
                std::make_shared<StatisticsWorker>(it.second, mCameraId, pipelineDepth);
            mListenerDeviceWorkers.push_back(statWorker.get());
            mMetaConfig.deviceWorkers.push_back(statWorker);
            mMetaConfig.pollableWorkers.push_back(statWorker);
            mMetaConfig.nodes.push_back(statWorker->getNode());
        } else if (it.first == IMGU_NODE_PARAM) {
            worker = std::make_shared<ParameterWorker>(it.second, mActiveStreams, mCameraId, pipelineDepth);
            mFirstWorkers.push_back(worker);
            videoConfig->deviceWorkers.push_back(worker); // parameters
        } else if (it.first == IMGU_NODE_STILL || it.first == IMGU_NODE_VIDEO) {
            std::shared_ptr<OutputFrameWorker> outWorker =
                std::make_shared<OutputFrameWorker>(it.second, mCameraId,
                    mStreamNodeMapping[it.first], it.first, pipelineDepth);
            videoConfig->deviceWorkers.push_back(outWorker);
            videoConfig->pollableWorkers.push_back(outWorker);
            videoConfig->nodes.push_back(outWorker->getNode());
            setStreamListeners(it.first, outWorker);
            //shutter event for non isys, zyc
            mListenerDeviceWorkers.push_back(outWorker.get());
        } else if (it.first == IMGU_NODE_VF_PREVIEW) {
            vfWorker = std::make_shared<OutputFrameWorker>(it.second, mCameraId,
                mStreamNodeMapping[it.first], it.first, pipelineDepth);
            setStreamListeners(it.first, vfWorker);
            //shutter event for non isys, zyc
            mListenerDeviceWorkers.push_back(vfWorker.get());
        } else if (it.first == IMGU_NODE_PV_PREVIEW) {
            pvWorker = std::make_shared<OutputFrameWorker>(it.second, mCameraId,
                mStreamNodeMapping[it.first], it.first, pipelineDepth);
            setStreamListeners(it.first, pvWorker);
            //shutter event for non isys, zyc
            mListenerDeviceWorkers.push_back(pvWorker.get());
        } else if (it.first == IMGU_NODE_RAW) {
            LOGW("Not implemented"); // raw
            continue;
        } else {
            LOGE("Unknown NodeName: %d", it.first);
            return UNKNOWN_ERROR;
        }
    }

    if (pvWorker.get()) {
        // Copy common part for still pipe, then add pv
        *stillConfig = *videoConfig;
        stillConfig->deviceWorkers.insert(stillConfig->deviceWorkers.begin(), pvWorker);
        stillConfig->pollableWorkers.insert(stillConfig->pollableWorkers.begin(), pvWorker);
        stillConfig->nodes.insert(stillConfig->nodes.begin(), pvWorker->getNode());

        if (mCurPipeConfig == videoConfig) {
            LOG1("%s: configure postview in advance", __FUNCTION__);
            pvWorker->configure(graphConfig);
        }
    }

    // Prepare for video pipe
    if (vfWorker.get()) {
        videoConfig->deviceWorkers.insert(videoConfig->deviceWorkers.begin(), vfWorker);
        videoConfig->pollableWorkers.insert(videoConfig->pollableWorkers.begin(), vfWorker);
        videoConfig->nodes.insert(videoConfig->nodes.begin(), vfWorker->getNode());

        // vf node provides source frame during still preview instead of pv node.
        if (pvWorker.get()) {
            setStreamListeners(IMGU_NODE_PV_PREVIEW, vfWorker);
        }

        if (mCurPipeConfig == stillConfig) {
            LOG1("%s: configure preview in advance", __FUNCTION__);
            vfWorker->configure(graphConfig);
        }
    }

    status_t ret = OK;
    for (const auto &it : mCurPipeConfig->deviceWorkers) {
        ret = (*it).configure(graphConfig);

        if (ret != OK) {
            LOGE("Failed to configure workers.");
            return ret;
        }
    }
    for (const auto &it : mMetaConfig.deviceWorkers) {
        ret = (*it).configure(graphConfig);

        if (ret != OK) {
            LOGE("Failed to configure meta workers.");
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

void ImguUnit::setStreamListeners(NodeTypes nodeName,
                                  std::shared_ptr<OutputFrameWorker>& source)
{
    for (const auto &it : mStreamListenerMapping) {
        if (it.second == nodeName) {
            LOG1("@%s stream %p listen to nodeName 0x%x",
                 __FUNCTION__, it.first, nodeName);
            source->addListener(it.first);
        }
    }
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

        LOG2("@%s: Req id %d,  Num outbufs %zu Num inbufs %zu",
             __FUNCTION__, reqId, outBufs ? outBufs->size() : 0, inBufs ? inBufs->size() : 0);

        ProcTaskMsg procMsg;
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
    Camera3Request *request = nullptr;


    LOG2("%s: pending size %zu,underwork.size(%d), state %d", __FUNCTION__, mMessagesPending.size(), mMessagesUnderwork.size(), mState);
    if (mMessagesPending.empty())
        return NO_ERROR;

    msg = mMessagesPending[0];
    mMessagesPending.erase(mMessagesPending.begin());

    // update and return metadata firstly
    request = msg->cbMetadataMsg.request;
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

    if (msg->cbMetadataMsg.updateMeta)
        updateProcUnitResults(*request, msg->pMsg.processingSettings);

    mMessagesUnderwork.push_back(msg);

    if (mFirstRequest) {
        status = kickstart();
        if (status != OK) {
            return status;
        }
    }

    // Reuqest do poll should after stream on,
    // otherwise the poll thread will notify a erreor event
    if (mNeedRestartPoll) {
        for(auto &it : mMetaConfig.deviceWorkers) {
            status |= (*it).prepareRun(msg);
        }
        status |= mPollerThreadMeta->pollRequest(request->getId(),
                                                 500000,
                                                 &(mMetaConfig.nodes));
        if (status != OK) {
            return status;
        }
        mNeedRestartPoll = false;
    }

    std::vector<std::shared_ptr<IDeviceWorker>>::iterator it = mCurPipeConfig->deviceWorkers.begin();
    for (;it != mCurPipeConfig->deviceWorkers.end(); ++it) {
        status = (*it)->prepareRun(msg);
        if (status != OK) {
            return status;
        }
    }

    mCurPipeConfig->nodes.clear();
    mRequestToWorkMap[request->getId()].clear();
    std::vector<std::shared_ptr<FrameWorker>>::iterator pollDevice = mCurPipeConfig->pollableWorkers.begin();
    for (;pollDevice != mCurPipeConfig->pollableWorkers.end(); ++pollDevice) {
        bool needsPolling = (*pollDevice)->needPolling();
        if (needsPolling) {
            mCurPipeConfig->nodes.push_back((*pollDevice)->getNode());
            mRequestToWorkMap[request->getId()].push_back((std::shared_ptr<IDeviceWorker>&)(*pollDevice));
        }
    }
    mRequestToWorkMap[request->getId()].push_back(*(mFirstWorkers.begin()));

    LOG2("%s:%d: poll request id(%d)", __func__, __LINE__, request->getId());
    status = mPollerThread->pollRequest(request->getId(),
                                        500000,
                                        &(mCurPipeConfig->nodes));

    return status;
}

status_t
ImguUnit::kickstart()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = OK;

    for (const auto &it : mCurPipeConfig->deviceWorkers) {
        status = (*it).startWorker();
        if (status != OK) {
            LOGE("Failed to start workers.");
            return status;
        }
    }
    for (const auto &it : mMetaConfig.deviceWorkers) {
        status = (*it).startWorker();
        if (status != OK) {
            LOGE("Failed to start meta workers.");
            return status;
        }
    }

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
ImguUnit::startProcessing(DeviceMessage pollmsg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    status_t status = OK;
    std::shared_ptr<V4L2VideoNode> *activeNodes = pollmsg.pollEvent.activeDevices;

    if (std::find(mMetaConfig.nodes.begin(), mMetaConfig.nodes.end(),
              (std::shared_ptr<V4L2DeviceBase>&)activeNodes[0]) != mMetaConfig.nodes.end()) {

        LOG2("%s:%d: mMetaConfig node polled, reqId(%d)", __func__, __LINE__, pollmsg.pollEvent.requestId);
        for(auto &it : mMetaConfig.deviceWorkers) {
            status |= (*it).run();
        }
        for(auto &it : mMetaConfig.deviceWorkers) {
            status |= (*it).postRun();
        }
        if (!mMessagesUnderwork.empty()) {
            std::shared_ptr<DeviceMessage> msg = *(mMessagesUnderwork.begin());
            Camera3Request *request = msg->cbMetadataMsg.request;
            for(auto &it : mMetaConfig.deviceWorkers) {
                status |= (*it).prepareRun(msg);
            }
            status |= mPollerThreadMeta->pollRequest(request->getId(),
                                                     500000,
                                                     &(mMetaConfig.nodes));
        } else {
            mNeedRestartPoll = true;
        }
        return status;
    }

    unsigned int reqId = pollmsg.pollEvent.requestId;
    std::vector<std::shared_ptr<IDeviceWorker>>::iterator it = mRequestToWorkMap[reqId].begin();
    for (;it != mRequestToWorkMap[reqId].end(); ++it) {
        std::shared_ptr<FrameWorker> worker = (std::shared_ptr<FrameWorker>&)(*it);
        status |= worker->asyncPollDone(*(mMessagesUnderwork.begin()), true);
    }

    it = mRequestToWorkMap[reqId].begin();
    for (;it != mRequestToWorkMap[reqId].end(); ++it) {
        status |= (*it)->run();
    }

    it = mRequestToWorkMap[reqId].begin();
    for (;it != mRequestToWorkMap[reqId].end(); ++it) {
        status |= (*it)->postRun();
    }
    mRequestToWorkMap.erase(reqId);

    //HACK: return metadata after updated it
    std::shared_ptr<DeviceMessage> msg = *(mMessagesUnderwork.begin());
    Camera3Request *request = msg->cbMetadataMsg.request;
    LOG2("%s: request %d done", __func__, request->getId());
    ICaptureEventListener::CaptureMessage outMsg;
    outMsg.data.event.reqId = request->getId();
    outMsg.data.event.type = ICaptureEventListener::CAPTURE_REQUEST_DONE;
    outMsg.id = ICaptureEventListener::CAPTURE_MESSAGE_ID_EVENT;
    for (const auto &listener : mListeners)
		listener->notifyCaptureEvent(&outMsg);

    request->mCallback->metadataDone(request, CONTROL_UNIT_PARTIAL_RESULT);
    mMessagesUnderwork.erase(mMessagesUnderwork.begin());

    return status;
}

status_t ImguUnit::notifyPollEvent(PollEventMessage *pollMsg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    if (pollMsg == nullptr || pollMsg->data.activeDevices == nullptr)
        return BAD_VALUE;

    // Common thread message fields for any case
    DeviceMessage msg;
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
            LOG2("@%s: %zu inactive nodes for request %u, retry poll", __FUNCTION__,
                                                                      pollMsg->data.inactiveDevices->size(),
                                                                      pollMsg->data.reqId);
            pollMsg->data.polledDevices->clear();
            *pollMsg->data.polledDevices = *pollMsg->data.inactiveDevices; // retry with inactive devices

            delete [] msg.pollEvent.activeDevices;

            return -EAGAIN;
        }

        {
            std::lock_guard<std::mutex> l(mFlushMutex);
            if (mFlushing)
                return OK;

            if (std::find(mMetaConfig.nodes.begin(), mMetaConfig.nodes.end(),
                          (std::shared_ptr<V4L2DeviceBase>&)msg.pollEvent.activeDevices[0]) != mMetaConfig.nodes.end()) {
                msg.id = MESSAGE_ID_POLL_META;
                mMessageQueue.send(&msg, MESSAGE_ID_POLL_META);
            } else {
                msg.id = MESSAGE_ID_POLL;
                mMessageQueue.send(&msg, MESSAGE_ID_POLL);
            }
        }

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

    status_t status = startProcessing(msg);

    delete [] msg.pollEvent.activeDevices;
    msg.pollEvent.activeDevices = nullptr;

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
        case MESSAGE_ID_POLL_META:
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

    {
        std::lock_guard<std::mutex> l(mFlushMutex);
        mFlushing = true;
    }

    mMessageQueue.remove(MESSAGE_ID_POLL);
    mMessageQueue.remove(MESSAGE_ID_POLL_META);

    return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
}

status_t
ImguUnit::handleMessageFlush(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    mPollerThread->flush(true);
    mPollerThreadMeta->flush(true);

    // Stop all video nodes
    if (mCurPipeConfig) {
        status_t status;
        for (const auto &it : mMetaConfig.deviceWorkers) {
            status = (*it).stopWorker();
            if (status != OK) {
                LOGE("Fail to stop wokers");
                return status;
            }
        }

        for (const auto &it : mCurPipeConfig->deviceWorkers) {
            status = (*it).stopWorker();
            if (status != OK) {
                LOGE("Fail to stop wokers");
                return status;
            }
        }
    }

    clearWorkers();
    return NO_ERROR;
}
} /* namespace camera2 */
} /* namespace android */
