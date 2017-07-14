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

#define LOG_TAG "StatisticsWorker"

#include "StatisticsWorker.h"
#include "statsConverter/ipu3-stats.h"

namespace android {
namespace camera2 {

StatisticsWorker::StatisticsWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId,
                     std::shared_ptr<SharedItemPool<ia_aiq_af_grid>> &afFilterBuffPool,
                     std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> &rgbsGridBuffPool):
        FrameWorker(node, cameraId, "StatisticsWorker"),
        mAfFilterBuffPool(afFilterBuffPool),
        mRgbsGridBuffPool(rgbsGridBuffPool)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mPollMe = true;
}

StatisticsWorker::~StatisticsWorker()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
}

status_t StatisticsWorker::configure(std::shared_ptr<GraphConfig> &/*config*/)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    FrameInfo frame;
    int page_size = getpagesize();
    frame.width = sizeof(imgu_abi_stats_3a) + page_size - (sizeof(imgu_abi_stats_3a) % page_size);
    frame.height = 1;
    frame.stride = frame.width;
    frame.format = V4L2_PIX_FMT_YUYV;
    status_t ret = setWorkerDeviceFormat(V4L2_BUF_TYPE_VIDEO_CAPTURE, frame);
    if (ret != OK)
        return ret;

    ret = setWorkerDeviceBuffers();
    if (ret != OK)
        return ret;

    ret = allocateWorkerBuffers();
    if (ret != OK)
        return ret;

    if (mCameraBuffers[0]->size() < frame.width) {
        LOGE("Stats buffer is not big enough");
        return UNKNOWN_ERROR;
    }

    return ret;
}

status_t StatisticsWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    mMsg = msg;

    status_t status = OK;

    status |= mNode->putFrame(&mBuffers[0]);

    if (status != OK) {
        LOGE("Failed to queue buffer to statistics device");
        return status;
    }

    return OK;
}

status_t StatisticsWorker::run()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    if (!mMsg) {
        LOGE("Message is not set - Fix the bug");
        return UNKNOWN_ERROR;
    }

    status_t status = OK;

    v4l2_buffer_info buf;
    CLEAR(buf);

    status = mNode->grabFrame(&buf);

    if (status != OK) {
        LOGE("Failed to dequeue buffer from statistics device");
        return status;
    }

    struct imgu_abi_stats_3a in;
    struct ipu3_stats_all_stats out;
    CLEAR(out);
    CLEAR(in);

    memcpy(&in, (void*)buf.vbuffer.m.userptr, sizeof(imgu_abi_stats_3a));

    ipu3_stats_get_3a(&out, &in);

    std::shared_ptr<ia_aiq_rgbs_grid> rgbsGrid = nullptr;
    std::shared_ptr<ia_aiq_af_grid> afGrid = nullptr;

    if (mAfFilterBuffPool == nullptr || mRgbsGridBuffPool == nullptr) {
        LOGE("mAfFilterBuffPool and mAfFilterBuffPool not configured");
        return BAD_VALUE;
    }

    status = mAfFilterBuffPool->acquireItem(afGrid);
    status |= mRgbsGridBuffPool->acquireItem(rgbsGrid);

    if (status != OK || afGrid.get() == nullptr || rgbsGrid.get() == nullptr) {
        LOGE("Failed to acquire 3A statistics memory from pools");
        return UNKNOWN_ERROR;
    }

    ICaptureEventListener::CaptureMessage outMsg;
    outMsg.id = ICaptureEventListener::CAPTURE_MESSAGE_ID_EVENT;
    outMsg.data.event.type = ICaptureEventListener::CAPTURE_EVENT_2A_STATISTICS;
    std::shared_ptr<IPU3CapturedStatistics> stats = std::make_shared<IPU3CapturedStatistics>();
    if (stats.get() == nullptr) {
        LOGE("Not enough memory to allocate stats");
        return NO_MEMORY;
    }

    stats->id = mMsg->pMsg.reqId;
    stats->rgbsGrid = *rgbsGrid.get();
    stats->afGrid = *afGrid.get();
    stats->pooledRGBSGrid = rgbsGrid;
    stats->pooledAfGrid = afGrid;
    stats->reset();
    stats->aiqStatsInputParams.frame_id = buf.vbuffer.sequence;
    stats->aiqStatsInputParams.frame_timestamp = (buf.vbuffer.timestamp.tv_sec * 1000000) + buf.vbuffer.timestamp.tv_usec;

    outMsg.data.event.stats = stats;

    ia_err ia_status = skycam_statistics_convert(&out.ia_css_4a_statistics, &stats->rgbsGrid, &stats->afGrid);

    if (ia_status != 0) {
        LOGE("skycam_statistics_convert failed, %d", status);
        return UNKNOWN_ERROR;
    }

    notifyListeners(&outMsg);

    return OK;
}

status_t StatisticsWorker::postRun()
{
    mMsg = nullptr;
    return OK;
}

} /* namespace camera2 */
} /* namespace android */
