/*
 * Copyright (C) 2016-2018 Intel Corporation.
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
#include "ImguUnit.h"
#include "LogHelper.h"
#include "PerformanceTraces.h"
#include "IPU3CapturedStatistics.h"
#include "workers/InputFrameWorker.h"
#include "workers/OutputFrameWorker.h"
#include "workers/StatisticsWorker.h"
#include "CameraMetadataHelper.h"

namespace android {
namespace camera2 {

ImguUnit::ImguUnit(int cameraId,
                   GraphConfigManager &gcm,
                   std::shared_ptr<MediaController> mediaCtl) :
        mCameraId(cameraId),
        mGCM(gcm),
        mMediaCtl(mediaCtl)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mActiveStreams.inputStream = nullptr;

    mRgbsGridBuffPool = std::make_shared<SharedItemPool<ia_aiq_rgbs_grid>>("RgbsGridBuffPool");
    mAfFilterBuffPool = std::make_shared<SharedItemPool<ia_aiq_af_grid>>("AfFilterBuffPool");

    status_t status = allocatePublicStatBuffers(PUBLIC_STATS_POOL_SIZE);
    if (status != NO_ERROR) {
        LOGE("Failed to allocate statistics, status: %d.", status);
    }
}

ImguUnit::~ImguUnit()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);

    mActiveStreams.blobStreams.clear();
    mActiveStreams.rawStreams.clear();
    mActiveStreams.yuvStreams.clear();

    cleanListener();

    for (size_t i = 0; i < GraphConfig::PIPE_MAX; i++) {
        mImguPipe[i] = nullptr;
    }

    freePublicStatBuffers();
    mRgbsGridBuffPool.reset();
    mAfFilterBuffPool.reset();
}

void ImguUnit::cleanListener()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    for (size_t i = 0; i < GraphConfig::PIPE_MAX; i++) {
        if (mImguPipe[i] != nullptr) {
            mImguPipe[i]->cleanListener();
        }
    }
    mListeners.clear();
}

status_t ImguUnit::attachListener(ICaptureEventListener *aListener)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    mListeners.push_back(aListener);
    return OK;
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
    LOG1("%s, numBufs %d", __FUNCTION__, numBufs);
    int status = mAfFilterBuffPool->init(numBufs);
    status |= mRgbsGridBuffPool->init(numBufs);
    if (status != OK) {
        LOGE("Failed to initialize 3A statistics pools");
        freePublicStatBuffers();
        return NO_MEMORY;
    }

    int maxGridSize = IPU3_MAX_STATISTICS_WIDTH * IPU3_MAX_STATISTICS_HEIGHT;
    std::shared_ptr<ia_aiq_rgbs_grid> rgbsGrid = nullptr;
    std::shared_ptr<ia_aiq_af_grid> afGrid = nullptr;

    for (int allocated = 0; allocated < numBufs; allocated++) {
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
    LOG1("%s", __FUNCTION__);
    status_t status = OK;
    if (!mAfFilterBuffPool->isFull() ||
        !mRgbsGridBuffPool->isFull()) {
        LOGE("We are leaking stats- AF:%s RGBS:%s",
                mAfFilterBuffPool->isFull()? "NO" : "YES",
                mRgbsGridBuffPool->isFull()? "NO" : "YES");
    }
    size_t availableItems = mAfFilterBuffPool->availableItems();
    std::shared_ptr<ia_aiq_af_grid> afGrid = nullptr;
    for (size_t i = 0; i < availableItems; i++) {
        status = mAfFilterBuffPool->acquireItem(afGrid);
        if (status == OK && afGrid.get() != nullptr) {
            DELETE_ARRAY_AND_NULLIFY(afGrid->filter_response_1);
            DELETE_ARRAY_AND_NULLIFY(afGrid->filter_response_2);
        } else {
            LOGE("Could not acquire filter response [%zu] for deletion - leak?", i);
        }
    }
    availableItems = mRgbsGridBuffPool->availableItems();
    std::shared_ptr<ia_aiq_rgbs_grid> rgbsGrid = nullptr;
    for (size_t i = 0; i < availableItems; i++) {
        status = mRgbsGridBuffPool->acquireItem(rgbsGrid);
        if (status == OK && rgbsGrid.get() != nullptr) {
            DELETE_ARRAY_AND_NULLIFY(rgbsGrid->blocks_ptr);
        } else {
            LOGE("Could not acquire RGBS grid [%zu] for deletion - leak?", i);
        }
    }
}

status_t ImguUnit::configStreams(std::vector<camera3_stream_t*> &activeStreams)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    mActiveStreams.blobStreams.clear();
    mActiveStreams.rawStreams.clear();
    mActiveStreams.yuvStreams.clear();
    mActiveStreams.inputStream = nullptr;

    for (size_t i = 0; i < GraphConfig::PIPE_MAX; i++) {
        mImguPipe[i] = nullptr;
    }

    bool hasIMPL = false;
    for (size_t i = 0; i < activeStreams.size(); ++i) {
        if (activeStreams[i]->stream_type == CAMERA3_STREAM_OUTPUT &&
            activeStreams[i]->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED) {
            hasIMPL = true;
            break;
        }
    }

    for (size_t i = 0; i < activeStreams.size(); ++i) {
        if (activeStreams.at(i)->stream_type == CAMERA3_STREAM_INPUT) {
            mActiveStreams.inputStream = activeStreams.at(i);
            continue;
        }

        switch (activeStreams.at(i)->format) {
        case HAL_PIXEL_FORMAT_BLOB:
            mActiveStreams.blobStreams.push_back(activeStreams.at(i));
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            if (hasIMPL &&
                activeStreams.at(i)->width > RESOLUTION_1080P_WIDTH &&
                activeStreams.at(i)->height > RESOLUTION_1080P_HEIGHT) {
                mActiveStreams.blobStreams.push_back(activeStreams.at(i));
            } else {
                mActiveStreams.yuvStreams.push_back(activeStreams.at(i));
            }
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            mActiveStreams.yuvStreams.push_back(activeStreams.at(i));
            break;
        default:
            LOGW("Unsupported stream format %x", activeStreams.at(i)->format);
            break;
        }
    }
    int blobNum = mActiveStreams.blobStreams.size();
    int yuvNum = mActiveStreams.yuvStreams.size();

    CheckError(blobNum > 2, BAD_VALUE, "Don't support blobNum %d", blobNum);
    CheckError(yuvNum > 2, BAD_VALUE, "Don't support yuvNum %d", yuvNum);

    status_t status = OK;
    if (yuvNum > 0) {
        mImguPipe[GraphConfig::PIPE_VIDEO] =
            std::unique_ptr<ImguPipe>(new ImguPipe(mCameraId, GraphConfig::PIPE_VIDEO, mMediaCtl, mListeners));

        // only statistics from VIDEO pipe is used to run 3A, register stats buffer for VIDEO pipe.
        status = mImguPipe[GraphConfig::PIPE_VIDEO]->configStreams(mActiveStreams.yuvStreams,
                          mGCM, mAfFilterBuffPool, mRgbsGridBuffPool);
        CheckError(status != OK, status, "Configure Video Pipe failed");
    }

    if (blobNum > 0) {
        mImguPipe[GraphConfig::PIPE_STILL] =
            std::unique_ptr<ImguPipe>(new ImguPipe(mCameraId, GraphConfig::PIPE_STILL, mMediaCtl, mListeners));

        status = mImguPipe[GraphConfig::PIPE_STILL]->configStreams(mActiveStreams.blobStreams,
                          mGCM, nullptr, nullptr);
        CheckError(status != OK, status, "Configure Still Pipe failed");
    }

    // Start works after configuring all IPU pipes
    for (size_t i = 0; i < GraphConfig::PIPE_MAX; i++) {
        if (mImguPipe[i] != nullptr) {
            status = mImguPipe[i]->startWorkers();
            CheckError(status != OK, status, "Start works failed, pipe %zu", i);
        }
    }

    return OK;
}

status_t ImguUnit::completeRequest(std::shared_ptr<ProcUnitSettings> &processingSettings,
                                   ICaptureEventListener::CaptureBuffers &captureBufs,
                                   bool updateMeta)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    LOG2("%s, updateMeta %d", __FUNCTION__, updateMeta);
    Camera3Request *request = processingSettings->request;
    if (CC_UNLIKELY(request == nullptr)) {
        LOGE("ProcUnit: nullptr request - BUG");
        return UNKNOWN_ERROR;
    }
    const std::vector<camera3_stream_buffer> *outBufs = request->getOutputBuffers();

    status_t status = OK;
    if (mImguPipe[GraphConfig::PIPE_VIDEO] != nullptr) {
        status = mImguPipe[GraphConfig::PIPE_VIDEO]->completeRequest(processingSettings,
                           captureBufs, updateMeta);
        CheckError(status != OK, status, "call video completeRequest failed");
    }

    if (mImguPipe[GraphConfig::PIPE_STILL] != nullptr) {
        bool hasStillBuffer = false;
        for (camera3_stream_buffer buf : *outBufs) {
            CameraStream *s = reinterpret_cast<CameraStream *>(buf.stream->priv);
            for (size_t index = 0; index < mActiveStreams.blobStreams.size(); ++index) {
                if (s->getStream() == mActiveStreams.blobStreams[index]) {
                    std::shared_ptr<CameraBuffer> buffer = request->findBuffer(s, false);
                    if (buffer != nullptr) {
                        hasStillBuffer = true;
                    } else {
                        LOGE("@%s, stream %p not found buffer", __FUNCTION__, s);
                    }
                }
            }
        }
        if (hasStillBuffer) {
            bool updateMetaInStill = mImguPipe[GraphConfig::PIPE_VIDEO] == nullptr ? true : false;
            status = mImguPipe[GraphConfig::PIPE_STILL]->completeRequest(processingSettings,
                               captureBufs, updateMetaInStill);
            CheckError(status != OK, status, "call still completeRequest failed");
        }
    }

    return OK;
}

status_t ImguUnit::flush(void)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    status_t status = OK;
    for (size_t i = 0; i < GraphConfig::PIPE_MAX; i++) {
        if (mImguPipe[i] != nullptr) {
            status |= mImguPipe[i]->flush();
        }
    }

    return status;
}

ImguUnit::ImguPipe::ImguPipe(int cameraId, GraphConfig::PipeType pipeType,
                             std::shared_ptr<MediaController> mediaCtl,
                             std::vector<ICaptureEventListener*> listeners) :
        mCameraId(cameraId),
        mPipeType(pipeType),
        mMediaCtlHelper(mediaCtl, nullptr),
        mState(IMGU_IDLE),
        mCameraThread("ImguThread" + std::to_string(pipeType)),
        mPollerThread(new PollerThread("ImguPollerThread" + std::to_string(pipeType))),
        mListeners(listeners),
        mFirstRequest(true),
        mFirstPollCallbacked(false)
{
    pthread_condattr_t attr;
    int ret = pthread_condattr_init(&attr);
    if (ret != 0) {
        LOGE("@%s, call pthread_condattr_init fails, ret:%d", __FUNCTION__, ret);
        pthread_condattr_destroy(&attr);
        return;
    }

    ret = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (ret != 0) {
        LOGE("@%s, call pthread_condattr_setclock fails, ret:%d", __FUNCTION__, ret);
        pthread_condattr_destroy(&attr);
        return;
    }

    ret = pthread_cond_init(&mFirstCond, &attr);
    if (ret != 0) {
        LOGE("@%s, call pthread_cond_init fails, ret:%d", __FUNCTION__, ret);
        pthread_condattr_destroy(&attr);
        return;
    }

    pthread_condattr_destroy(&attr);

    ret = pthread_mutex_init(&mFirstLock, nullptr);
    CheckError(ret != 0, VOID_VALUE, "@%s, call pthread_cond_init fails, ret:%d", __FUNCTION__, ret);

    if (!mCameraThread.Start()) {
        LOGE("pipe %d thread failed to start", pipeType);
        return;
    }

    LOG1("%s, Pipe Type %d", __FUNCTION__, mPipeType);
}

ImguUnit::ImguPipe::~ImguPipe()
{
    LOG1("%s, Pipe Type %d", __FUNCTION__, mPipeType);
    int ret = pthread_mutex_destroy(&mFirstLock);
    if (ret != 0)
        LOGE("@%s, call pthread_mutex_destroy fails, ret: %d", __FUNCTION__, ret);

    ret = pthread_cond_destroy(&mFirstCond);
    if (ret != 0)
        LOGE("@%s, call pthread_cond_destroy fails, ret: %d", __FUNCTION__, ret);

    status_t status = NO_ERROR;

    if (mPollerThread) {
        status |= mPollerThread->requestExitAndWait();
        mPollerThread.reset();
    }

    mCameraThread.Stop();

    if (mMessagesUnderwork.size())
        LOGW("There are messages that are not processed %zu:", mMessagesUnderwork.size());
    if (mMessagesPending.size())
        LOGW("There are pending messages %zu:", mMessagesPending.size());

    clearWorkers();
}

void ImguUnit::ImguPipe::clearWorkers()
{
    LOG2("%s pipe type %d", __FUNCTION__, mPipeType);
    mPipeConfig.deviceWorkers.clear();
    mPipeConfig.pollableWorkers.clear();
    mPipeConfig.nodes.clear();

    mFirstWorkers.clear();
    mListenerDeviceWorkers.clear();
}

void ImguUnit::ImguPipe::cleanListener()
{
    LOG2("%s pipe type %d", __FUNCTION__, mPipeType);

    std::vector<ICaptureEventSource*>::iterator it = mListenerDeviceWorkers.begin();
    for (;it != mListenerDeviceWorkers.end(); ++it) {
        (*it)->cleanListener();
    }
    mListeners.clear();
}

status_t ImguUnit::ImguPipe::configStreams(std::vector<camera3_stream_t*> &streams,
                       GraphConfigManager &gcm,
                       std::shared_ptr<SharedItemPool<ia_aiq_af_grid>> afGridBuffPool,
                       std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> rgbsGridBuffPool)
{
    LOG1("%s, Pipe Type %d, streams number:%lu", __FUNCTION__, mPipeType, streams.size());
    mFirstRequest = true;

    IStreamConfigProvider::MediaType mediaType = GraphConfig::PIPE_STILL == mPipeType ?
                           IStreamConfigProvider::IMGU_STILL : IStreamConfigProvider::IMGU_VIDEO;

    std::shared_ptr<GraphConfig> graphConfig = gcm.getBaseGraphConfig(mediaType);
    if (CC_UNLIKELY(graphConfig.get() == nullptr)) {
        LOGE("ERROR: Graph config is nullptr");
        return UNKNOWN_ERROR;
    }

    status_t status = mMediaCtlHelper.configure(gcm, mediaType);
    CheckError(status != OK, status, "failed to configure video MediaCtlHelper");

    clearWorkers();

    mConfiguredNodesPerName = mMediaCtlHelper.getConfiguredNodesPerName(mediaType);
    CheckError(mConfiguredNodesPerName.size() == 0, UNKNOWN_ERROR, "No nodes present");

    status = mapStreamWithDeviceNode(streams);
    CheckError(status != OK, status, "failed to map stream with Device node");

    status = createProcessingTasks(graphConfig, afGridBuffPool, rgbsGridBuffPool);
    CheckError(status != NO_ERROR, status, "Tasks creation failed (ret = %d)", status);

    status = mPollerThread->init(mPipeConfig.nodes,
                                 this, POLLPRI | POLLIN | POLLOUT | POLLERR, false);
    CheckError(status != NO_ERROR, status, "PollerThread init failed (ret = %d)", status);

    return OK;
}

status_t ImguUnit::ImguPipe::startWorkers()
{
    LOG1("%s, Pipe Type %d", __FUNCTION__, mPipeType);

    for (const auto &it : mPipeConfig.deviceWorkers) {
        status_t status = (*it).startWorker();
        CheckError(status != OK, status, "Failed to start workers, status %d", status);
    }

    return OK;
}

status_t ImguUnit::ImguPipe::mapStreamWithDeviceNode(std::vector<camera3_stream_t*> &streams)
{
    int streamNum = streams.size();
    LOG1("%s pipe type %d, streamNum %d", __FUNCTION__, mPipeType, streamNum);
    CheckError(streamNum == 0, UNKNOWN_ERROR, "streamNum is 0");

    mStreamNodeMapping.clear();
    mStreamListenerMapping.clear();

    if (GraphConfig::PIPE_VIDEO == mPipeType) {
        int videoIdx = (streamNum == 2) ? 0 : -1;
        int previewIdx = (streamNum == 2) ? 1 : 0;

        mStreamNodeMapping[IMGU_NODE_PREVIEW] = streams[previewIdx];
        LOG1("@%s, %d stream %p size preview: %dx%d, format %s", __FUNCTION__,
             previewIdx, streams[previewIdx],
             streams[previewIdx]->width, streams[previewIdx]->height,
             METAID2STR(android_scaler_availableFormats_values,
                        streams[previewIdx]->format));
        if (videoIdx >= 0) {
            mStreamNodeMapping[IMGU_NODE_VIDEO] = streams[videoIdx];
            LOG1("@%s, %d stream %p size video: %dx%d, format %s", __FUNCTION__,
                 videoIdx, streams[videoIdx],
                 streams[videoIdx]->width, streams[videoIdx]->height,
                 METAID2STR(android_scaler_availableFormats_values,
                            streams[videoIdx]->format));
        }
    } else if (GraphConfig::PIPE_STILL == mPipeType) {
        mStreamNodeMapping[IMGU_NODE_STILL] = streams[0];
        LOG1("@%s, blob stream %p size video: %dx%d, format %s", __FUNCTION__,
             streams[0], streams[0]->width, streams[0]->height,
             METAID2STR(android_scaler_availableFormats_values,
                        streams[0]->format));

        if (streams.size() == 2) {
            mStreamListenerMapping[streams[1]] = IMGU_NODE_STILL;
        }
    }

    return OK;
}

/**
 * Create the processing tasks
 * Processing tasks are:
 *  - video task (wraps video pipeline)
 *  - capture task (wraps still capture)
 *  - raw bypass (not done yet)
 *
 * \param[in] graphConfig Configuration of the base graph
 */
status_t ImguUnit::ImguPipe::createProcessingTasks(std::shared_ptr<GraphConfig> graphConfig,
                           std::shared_ptr<SharedItemPool<ia_aiq_af_grid>> afGridBuffPool,
                           std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> rgbsGridBuffPool)
{
    LOG1("%s pipe type %d", __FUNCTION__, mPipeType);
    const camera_metadata_t *meta = PlatformData::getStaticMetadata(mCameraId);
    camera_metadata_ro_entry entry;
    CLEAR(entry);
    if (meta)
        entry = MetadataHelper::getMetadataEntry(meta, ANDROID_REQUEST_PIPELINE_MAX_DEPTH);
    size_t pipelineDepth = entry.count == 1 ? entry.data.u8[0] : 1;

    for (const auto &it : mConfiguredNodesPerName) {
        std::shared_ptr<FrameWorker> worker = nullptr;
        if (it.first == IMGU_NODE_INPUT) {
            worker = std::make_shared<InputFrameWorker>(it.second, mCameraId, pipelineDepth);
            mPipeConfig.deviceWorkers.push_back(worker); // Input frame;
            mPipeConfig.pollableWorkers.push_back(worker);
            mPipeConfig.nodes.push_back(worker->getNode()); // Nodes are added for pollthread init
            mFirstWorkers.push_back(worker);
        } else if (it.first == IMGU_NODE_STAT) {
            std::shared_ptr<StatisticsWorker> statWorker =
                std::make_shared<StatisticsWorker>(it.second, mCameraId, mPipeType,
                    afGridBuffPool, rgbsGridBuffPool);
            mListenerDeviceWorkers.push_back(statWorker.get());
            mPipeConfig.deviceWorkers.push_back(statWorker);
            mPipeConfig.pollableWorkers.push_back(statWorker);
            mPipeConfig.nodes.push_back(statWorker->getNode());
        } else if (it.first == IMGU_NODE_PARAM) {
            worker = std::make_shared<ParameterWorker>(it.second, mCameraId, mPipeType);
            mFirstWorkers.push_back(worker);
            mPipeConfig.deviceWorkers.push_back(worker); // parameters
        } else if (it.first == IMGU_NODE_STILL || it.first == IMGU_NODE_VIDEO
                || it.first == IMGU_NODE_PREVIEW) {
            std::shared_ptr<OutputFrameWorker> outWorker =
                std::make_shared<OutputFrameWorker>(it.second, mCameraId,
                    mStreamNodeMapping[it.first], it.first, pipelineDepth);
            mPipeConfig.deviceWorkers.push_back(outWorker);
            mPipeConfig.pollableWorkers.push_back(outWorker);
            mPipeConfig.nodes.push_back(outWorker->getNode());

            for (const auto& l : mStreamListenerMapping) {
                if (l.second == it.first) {
                    LOG1("@%s stream %p listen to nodeName 0x%x", __FUNCTION__, l.first, it.first);
                    outWorker->addListener(l.first);
                }
            }
        } else if (it.first == IMGU_NODE_RAW) {
            LOGW("RAW is not implemented"); // raw
            continue;
        } else {
            LOGE("Unknown NodeName: %d", it.first);
            return UNKNOWN_ERROR;
        }
    }

    for (const auto &it : mPipeConfig.deviceWorkers) {
        status_t status = (*it).configure(graphConfig);
        CheckError(status != OK, status, "Failed to configure workers, status %d.", status);
    }

    std::vector<ICaptureEventSource*>::iterator it = mListenerDeviceWorkers.begin();
    for (;it != mListenerDeviceWorkers.end(); ++it) {
        for (const auto &listener : mListeners) {
            (*it)->attachListener(listener);
        }
    }

    return OK;
}

status_t
ImguUnit::ImguPipe::completeRequest(std::shared_ptr<ProcUnitSettings> &processingSettings,
                                    ICaptureEventListener::CaptureBuffers &captureBufs,
                                    bool updateMeta)
{
    LOG2("%s, pipe type %d, updateMeta %d", __FUNCTION__, mPipeType, updateMeta);
    Camera3Request *request = processingSettings->request;
    if (CC_UNLIKELY(request == nullptr)) {
        LOGE("ProcUnit: nullptr request - BUG");
        return UNKNOWN_ERROR;
    }
    const std::vector<camera3_stream_buffer> *outBufs = request->getOutputBuffers();
    int reqId = request->getId();

    LOG2("@%s: Req id %d,  Num outbufs %lu Num inbufs %d",
         __FUNCTION__, reqId, outBufs ? outBufs->size() : 0, (request->hasInputBuf() ? 1 : 0));

    if (captureBufs.rawNonScaledBuffer.get() != nullptr) {
        LOG2("Using Non Scaled Buffer %p for req id %d",
             reinterpret_cast<void*>(captureBufs.rawNonScaledBuffer->Userptr(0)), reqId);
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
    base::Callback<status_t()> closure =
            base::Bind(&ImguUnit::ImguPipe::handleCompleteReq, base::Unretained(this),
                       base::Passed(std::move(msg)));
    mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
    return NO_ERROR;
}

status_t ImguUnit::ImguPipe::handleCompleteReq(DeviceMessage msg)
{
    LOG2("%s, msg.id %d, pipe type %d", __FUNCTION__, static_cast<int>(msg.id), mPipeType);

    Camera3Request *request = msg.cbMetadataMsg.request;
    if (request == nullptr) {
        LOGE("Request is nullptr");
        return BAD_VALUE;
    }

    LOG2("order %s:enqueue for Req id %d, ", __FUNCTION__, request->getId());
    std::shared_ptr<DeviceMessage> tmp = std::make_shared<DeviceMessage>(msg);
    mMessagesPending.push_back(tmp);

    status_t status = processNextRequest();
    if (status != NO_ERROR)
        LOGE("error %d in handling message: %d", status, static_cast<int>(msg.id));

    return status;
}

status_t ImguUnit::ImguPipe::processNextRequest()
{
    LOG2("%s: pending size %zu, state %d, pipe type %d", __FUNCTION__,
          mMessagesPending.size(), mState, mPipeType);
    if (mMessagesPending.empty())
        return NO_ERROR;

    if (mState == IMGU_RUNNING) {
        LOGD("IMGU busy - message put into a waiting queue");
        return NO_ERROR;
    }

    std::shared_ptr<DeviceMessage> msg = mMessagesPending[0];
    CheckError(msg == nullptr, BAD_VALUE, "@%s: mMessagePending[0] is nullptr", __FUNCTION__);
    mMessagesPending.erase(mMessagesPending.begin());

    // update and return metadata firstly
    Camera3Request *request = msg->cbMetadataMsg.request;
    CheckError(request == nullptr, BAD_VALUE, "Request is nullptr");

    if (GraphConfig::PIPE_STILL == mPipeType) mFirstRequest = true;
    LOG2("@%s:handleExecuteReq for Req id %d, ", __FUNCTION__, request->getId());

    if (msg->cbMetadataMsg.updateMeta) {
        updateProcUnitResults(*request, msg->pMsg.processingSettings);
        request->mCallback->metadataDone(request, CONTROL_UNIT_PARTIAL_RESULT);
    }

    mMessagesUnderwork.push_back(msg);

    status_t status = NO_ERROR;
    if (mFirstRequest) {
        status = kickstart(request);
        CheckError(status != OK, status, "failed to kick start, status %d", status);
    }

    std::vector<std::shared_ptr<IDeviceWorker>>::iterator it = mPipeConfig.deviceWorkers.begin();
    for (;it != mPipeConfig.deviceWorkers.end(); ++it) {
        status = (*it)->prepareRun(msg);
        CheckError(status != OK, status, "failed to prepare works, status %d", status);
    }

    mPipeConfig.nodes.clear();
    std::vector<std::shared_ptr<FrameWorker>>::iterator pollDevice = mPipeConfig.pollableWorkers.begin();
    for (;pollDevice != mPipeConfig.pollableWorkers.end(); ++pollDevice) {
        bool needsPolling = (*pollDevice)->needPolling();
        if (needsPolling) {
            mPipeConfig.nodes.push_back((*pollDevice)->getNode());
        }
    }

    status = mPollerThread->pollRequest(request->getId(),
                                        IPU3_EVENT_POLL_TIMEOUT,
                                        &(mPipeConfig.nodes));
    CheckError(status != OK, status, "failed to poll request, status %d", status);

    mState = IMGU_RUNNING;

    return status;
}

status_t ImguUnit::ImguPipe::kickstart(Camera3Request* request)
{
    LOG1("%s, pipe type %d", __FUNCTION__, mPipeType);
    status_t status = OK;

    std::vector<std::shared_ptr<cros::V4L2Device>> firstNodes;
    std::vector<std::shared_ptr<IDeviceWorker>>::iterator firstit = mFirstWorkers.begin();
    firstit = mFirstWorkers.begin();
    for (;firstit != mFirstWorkers.end(); ++firstit) {
        status |= (*firstit)->prepareRun(mMessagesUnderwork[0]);
        /* skip polling the node that doesn't queue buffer when test pattern mode is on */
        if (!(*firstit)->needPolling() &&
            mMessagesUnderwork[0]->pMsg.processingSettings->captureSettings->testPatternMode
            != ANDROID_SENSOR_TEST_PATTERN_MODE_OFF)
            continue;
        else
            firstNodes.push_back((*firstit)->getNode());
    }

    CheckError(status != OK, status, "@%s, fail to call prepareRun", __FUNCTION__);

    /* poll first Imgu frame */
    pthread_mutex_lock(&mFirstLock);
    status = mPollerThread->pollRequest(request->getId(), IPU3_EVENT_POLL_TIMEOUT, &firstNodes);
    if (status != OK) {
        LOGE("@%s, poll request for first frame failed", __FUNCTION__);
        pthread_mutex_unlock(&mFirstLock);
        return UNKNOWN_ERROR;
    }

    struct timespec ts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += IPU3_EVENT_POLL_TIMEOUT/1000;
    int ret = 0;

    while (!mFirstPollCallbacked && !ret) {
        ret = pthread_cond_timedwait(&mFirstCond, &mFirstLock, &ts);
    }
    if (ret != 0) {
        LOGE("@%s, call pthread_cond_timedwait failes, ret: %d", __FUNCTION__, ret);
        pthread_mutex_unlock(&mFirstLock);
        return UNKNOWN_ERROR;
    }
    mFirstPollCallbacked = false;
    pthread_mutex_unlock(&mFirstLock);

    firstit = mFirstWorkers.begin();
    for (;firstit != mFirstWorkers.end(); ++firstit) {
        status |= (*firstit)->run();
        CheckError(status != OK, status, "failed to run works, status %d", status);
    }

    firstit = mFirstWorkers.begin();
    for (;firstit != mFirstWorkers.end(); ++firstit) {
        status |= (*firstit)->postRun();
        CheckError(status != OK, status, "failed to post-run works, status %d", status);
    }

    return status;
}

status_t
ImguUnit::ImguPipe::updateProcUnitResults(Camera3Request &request,
                                          std::shared_ptr<ProcUnitSettings> settings)
{
    LOG2("%s, pipe type %d", __FUNCTION__, mPipeType);
    status_t status = NO_ERROR;

    CameraMetadata *ctrlUnitResult = request.getPartialResultBuffer(CONTROL_UNIT_PARTIAL_RESULT);
    CheckError(ctrlUnitResult == nullptr, UNKNOWN_ERROR,
               "Failed to retrieve Metadata buffer for reqId = %d", request.getId());

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
status_t ImguUnit::ImguPipe::startProcessing()
{
    LOG2("%s, pipe type %d", __FUNCTION__, mPipeType);

    /* skip processing the first frame */
    if (mFirstRequest) {
        mFirstRequest = false;
        return OK;
    }

    status_t status = OK;
    std::vector<std::shared_ptr<IDeviceWorker>>::iterator it = mPipeConfig.deviceWorkers.begin();
    for (;it != mPipeConfig.deviceWorkers.end(); ++it) {
        status |= (*it)->run();
    }

    it = mPipeConfig.deviceWorkers.begin();
    for (;it != mPipeConfig.deviceWorkers.end(); ++it) {
        status |= (*it)->postRun();
    }

    mMessagesUnderwork.erase(mMessagesUnderwork.begin());

    mState = IMGU_IDLE;

    return status;
}

status_t ImguUnit::ImguPipe::notifyPollEvent(PollEventMessage *pollMsg)
{
    LOG2("%s pipe type %d", __FUNCTION__, mPipeType);
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

        msg.pollEvent.numDevices = numDevices;
        msg.pollEvent.polledDevices = numPolledDevices;

        if (pollMsg->data.activeDevices->size() != pollMsg->data.polledDevices->size()) {
            LOG2("@%s: %zu inactive nodes for request %u, retry poll", __FUNCTION__,
                  pollMsg->data.inactiveDevices->size(), pollMsg->data.reqId);
            pollMsg->data.polledDevices->clear();
            // retry with inactive devices
            *pollMsg->data.polledDevices = *pollMsg->data.inactiveDevices;
            return -EAGAIN;
        }

        if (mFirstRequest) {
            pthread_mutex_lock(&mFirstLock);
            mFirstPollCallbacked = true;
            int ret = pthread_cond_signal(&mFirstCond);
            if (ret != 0) {
                LOGE("@%s, call pthread_cond_signal fails, ret: %d", __FUNCTION__, ret);
                pthread_mutex_unlock(&mFirstLock);
                return UNKNOWN_ERROR;
            }
            pthread_mutex_unlock(&mFirstLock);
        }
        base::Callback<status_t()> closure =
                base::Bind(&ImguUnit::ImguPipe::handlePoll, base::Unretained(this),
                           base::Passed(std::move(msg)));
        mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);

    } else if (pollMsg->id == POLL_EVENT_ID_ERROR) {
        LOGE("Device poll failed");
        // For now, set number of device to zero in error case
        msg.pollEvent.numDevices = 0;
        msg.pollEvent.polledDevices = 0;
        base::Callback<status_t()> closure =
                base::Bind(&ImguUnit::ImguPipe::ImguPipe::handlePoll, base::Unretained(this),
                           base::Passed(std::move(msg)));
        mCameraThread.PostTaskAsync<status_t>(FROM_HERE, closure);
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
ImguUnit::ImguPipe::updateMiscMetadata(CameraMetadata &procUnitResults,
                                       std::shared_ptr<const ProcUnitSettings> settings) const
{
    LOG2("%s, pipe type %d", __FUNCTION__, mPipeType);
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
    //# ANDROID_REPROCESS_EFFECTIVE_EXPOSURE_FACTOR reprocess.effectiveExposureFactor done
    if (settings->captureSettings->effectiveExposureFactor > 0.0) {
        procUnitResults.update(ANDROID_REPROCESS_EFFECTIVE_EXPOSURE_FACTOR,
                           &settings->captureSettings->effectiveExposureFactor, 1);
    }
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
ImguUnit::ImguPipe::updateDVSMetadata(CameraMetadata &procUnitResults,
                                      std::shared_ptr<const ProcUnitSettings> settings) const
{
    LOG2("%s, pipe type %d", __FUNCTION__, mPipeType);
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

status_t ImguUnit::ImguPipe::handlePoll(DeviceMessage msg)
{
    LOG2("%s, pipe type %d, req id %u", __FUNCTION__, mPipeType, msg.pollEvent.requestId);

    status_t status = startProcessing();
    if (status == NO_ERROR)
        status = processNextRequest();

    if (status != NO_ERROR)
        LOGE("error %d in handling message: %d",
             status, static_cast<int>(msg.id));

    return status;
}

status_t
ImguUnit::ImguPipe::flush(void)
{
    LOG2("%s pipe type %d", __FUNCTION__, mPipeType);
    status_t status = NO_ERROR;
    base::Callback<status_t()> closure =
            base::Bind(&ImguUnit::ImguPipe::handleFlush, base::Unretained(this));
    mCameraThread.PostTaskSync<status_t>(FROM_HERE, closure, &status);
    return status;
}

status_t ImguUnit::ImguPipe::handleFlush(void)
{
    LOG2("%s pipe type %d", __FUNCTION__, mPipeType);
    mPollerThread->flush(true);

    clearWorkers();
    return NO_ERROR;
}

} /* namespace camera2 */
} /* namespace android */
