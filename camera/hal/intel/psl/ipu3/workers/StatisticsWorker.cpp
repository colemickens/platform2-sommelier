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
#include "LogHelper.h"
#include "NodeTypes.h"

namespace android {
namespace camera2 {

const unsigned int STAT_WORK_BUFFERS = 1;

StatisticsWorker::StatisticsWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId,
                     std::shared_ptr<SharedItemPool<ia_aiq_af_grid>> &afFilterBuffPool,
                     std::shared_ptr<SharedItemPool<ia_aiq_rgbs_grid>> &rgbsGridBuffPool):
        FrameWorker(node, cameraId, STAT_WORK_BUFFERS, "StatisticsWorker"),
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
    status_t ret = setWorkerDeviceFormat(frame);
    if (ret != OK)
        return ret;

    ret = setWorkerDeviceBuffers(getDefaultMemoryType(IMGU_NODE_STAT));
    if (ret != OK)
        return ret;

    ret = allocateWorkerBuffers();
    if (ret != OK)
        return ret;

    if (mCameraBuffers[0]->size() < mFormat.sizeimage()) {
        LOGE("Stats buffer is not big enough");
        return UNKNOWN_ERROR;
    }

    mIndex = 0;

    return ret;
}

status_t StatisticsWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    mMsg = msg;

    status_t status = OK;

    status |= mNode->putFrame(mBuffers[mIndex]);

    if (status != OK) {
        LOGE("Failed to queue buffer to statistics device");
        return status;
    }

    mIndex = (mIndex + 1) % mPipelineDepth;

    return OK;
}

status_t StatisticsWorker::run()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);
    if (!mMsg) {
        LOGE("Message is not set - Fix the bug");
        return UNKNOWN_ERROR;
    }

    #define DUMP_INTERVAL 10
    status_t status = OK;

    V4L2BufferInfo buf;

    status = mNode->grabFrame(&buf);

    if (status < 0) {
        LOGE("Failed to dequeue buffer from statistics device");
        return status;
    }

    struct imgu_abi_stats_3a in;
    struct ipu3_stats_all_stats out;
    CLEAR(out);
    CLEAR(in);

    MEMCPY_S(&in, sizeof(in), mCameraBuffers[0]->data(), sizeof(imgu_abi_stats_3a));

    ipu3_stats_get_3a(&out, &in);

    if (mAfFilterBuffPool == nullptr || mRgbsGridBuffPool == nullptr) {
        LOGE("mAfFilterBuffPool and mAfFilterBuffPool not configured");
        return BAD_VALUE;
    }

    std::shared_ptr<ia_aiq_rgbs_grid> rgbsGrid = nullptr;
    std::shared_ptr<ia_aiq_af_grid> afGrid = nullptr;
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
    stats->pooledRGBSGrid = rgbsGrid;
    stats->pooledAfGrid = afGrid;

    CLEAR(stats->aiqStatsInputParams);

    stats->rgbsGridArray[0] = stats->pooledRGBSGrid.get();
    stats->aiqStatsInputParams.rgbs_grids = stats->rgbsGridArray;
    stats->aiqStatsInputParams.num_rgbs_grids
        = sizeof(stats->rgbsGridArray) / sizeof(stats->rgbsGridArray[0]);

    stats->afGridArray[0] = stats->pooledAfGrid.get();
    stats->aiqStatsInputParams.af_grids = stats->afGridArray;
    stats->aiqStatsInputParams.num_af_grids
        = sizeof(stats->afGridArray) / sizeof(stats->afGridArray[0]);

    stats->aiqStatsInputParams.frame_af_parameters = &stats->af_results;
    stats->aiqStatsInputParams.hdr_rgbs_grid = nullptr;
    stats->aiqStatsInputParams.depth_grids  = nullptr;
    stats->aiqStatsInputParams.num_depth_grids = 0;

    stats->aiqStatsInputParams.frame_id = mMsg->pMsg.rawNonScaledBuffer->v4l2Buf.sequence();
    stats->aiqStatsInputParams.frame_timestamp
        = (buf.vbuffer.timestamp().tv_sec * 1000000) + buf.vbuffer.timestamp().tv_usec;

    stats->frameSequence = mMsg->pMsg.rawNonScaledBuffer->v4l2Buf.sequence();
    LOG2("sensor frame sequence %u", stats->frameSequence);

    outMsg.data.event.stats = stats;

    if (0 == (stats->aiqStatsInputParams.frame_id % DUMP_INTERVAL)) {
        if (gRgbsGridDump) {
            std::string filename = CAMERA_OPERATION_FOLDER;
            filename += "rgbs_grid";
            writeRgbsGridToBmp(filename.c_str(), out.ia_css_4a_statistics, buf.vbuffer.sequence());
        }

        if (gAfGridDump) {
            std::string filename = CAMERA_OPERATION_FOLDER;
            filename += "af_grid";
            writeAfGridToBmp(filename.c_str(), out.ia_css_4a_statistics, buf.vbuffer.sequence());
        }
    }

    ia_err ia_status = intel_skycam_statistics_convert(out.ia_css_4a_statistics, rgbsGrid.get(), afGrid.get());
    if (ia_status != 0) {
        LOGE("skycam_statistics_convert failed, %d", status);
        return UNKNOWN_ERROR;
    }

    if (LogHelper::isDebugTypeEnable(CAMERA_DEBUG_LOG_AIQ)) {
        if (rgbsGrid.get() != nullptr
                && rgbsGrid.get()->blocks_ptr != nullptr
                && rgbsGrid.get()->grid_width > 0
                && rgbsGrid.get()->grid_height > 0) {
            int sumLuma = 0;
            size_t size = rgbsGrid.get()->grid_width * rgbsGrid.get()->grid_height;
            rgbs_grid_block *ptr = rgbsGrid.get()->blocks_ptr;
            for (size_t i = 0; i < size; ++i) {
                sumLuma += (ptr[i].avg_r + (ptr[i].avg_gb+ptr[i].avg_gr )/2 + ptr[i].avg_b) / 3;
            }
            LOGAIQ("%s, frame %u RGBS y_mean %lu, widthxheight = [%dx%d]", __FUNCTION__,
                stats->frameSequence, sumLuma / size,
                rgbsGrid.get()->grid_width, rgbsGrid.get()->grid_height);
        }
    }

    notifyListeners(&outMsg);

    return OK;
}

status_t StatisticsWorker::postRun()
{
    mMsg = nullptr;
    return OK;
}

void StatisticsWorker::writeBmp(const std::string &filename,
    const ia_css_4a_statistics &input_params,
    unsigned int grid_width, unsigned int grid_height,
    unsigned int cur_grid_filter_num, unsigned int flag)
{
    FILE *bmpFile = fopen(filename.c_str(), "wb");
    if (!bmpFile) {
        LOGE("@%s, Failed to open Bmp file:%s from file system!, error:%s",
            __FUNCTION__, filename.c_str(), strerror(errno));
        return;
    }

    size_t bmpSize = sizeof(BMPFILETYPE) + sizeof(BMPFILEHEADER) + \
                                 sizeof(BMPINFOHEADER) + ALGIN4(grid_width * 3) * grid_height;
    std::unique_ptr<unsigned char[]> bmpBuffer(new unsigned char[bmpSize]);

    LOG2("stat bmp buffer size %lu grid %ux%u", bmpSize, grid_width, grid_height);

    if (flag == RGBS_GRID_TO_BMP) {
        gridToBmp(input_params.data, grid_width, grid_height, bmpBuffer.get());
    } else if (flag == AF_GRID_TO_BMP) {
        afGridFilterResponseToBmp(input_params.data, bmpBuffer.get(),
                                 grid_width, grid_height, cur_grid_filter_num);
    }
    fwrite(bmpBuffer.get(), 1, bmpSize, bmpFile);

    fclose(bmpFile);
}

void StatisticsWorker::writeRgbsGridToBmp(const std::string &rgbsFilename,
    const ia_css_4a_statistics &input_params,
    unsigned int frame_counter)
{
    int grid_width = input_params.stats_4a_config->awb_grd_config.grid_width;
    int grid_height = input_params.stats_4a_config->awb_grd_config.grid_height;
    int rgbs_grids_size = grid_width * grid_height;
    int rgbs_flag = RGBS_GRID_TO_BMP;

    if (input_params.data == nullptr) {
        LOGE("Input parameter is invalid !");
        return;
    }

    if (input_params.data) {
        std::string filename = rgbsFilename;
        filename += "_" + std::to_string(frame_counter) + ".bmp";
        writeBmp(filename, input_params, grid_width, grid_height, 0, rgbs_flag);
    }
}

void StatisticsWorker::writeAfGridToBmp(const std::string &afFilename,
    const ia_css_4a_statistics &input_params,
    unsigned int frame_counter)
{
    unsigned int grid_width = input_params.stats_4a_config->af_grd_config.grid_width;
    unsigned int grid_height = input_params.stats_4a_config->af_grd_config.grid_height;
    int af_flag = AF_GRID_TO_BMP;

    if (input_params.data == nullptr) {
        LOGE("Input parameter is invalid !");
        return;
    }

    for (unsigned int j = 1; j <= GRID_FILTER_NUM; ++j) {
        std::string filename = afFilename;
        filename += "_FR" + std::to_string(j) + "_" + std::to_string(frame_counter) + ".bmp";
        writeBmp(filename, input_params, grid_width, grid_height, j, af_flag);
    }
}

status_t StatisticsWorker::afGridFilterResponseToBmp(const stats_4a_public_raw_buffer *raw_buffer,
    unsigned char *output_ptr,
    unsigned int grid_width,
    unsigned int grid_height,
    int filter_num)
{
    if (filter_num <= 0 || filter_num > GRID_FILTER_NUM)
        return BAD_VALUE;

    const af_public_raw_buffer_t  *af_raw_buffer = &raw_buffer->af_raw_buffer;
    unsigned int i, j;
    unsigned int width = grid_width;
    unsigned int height = grid_height;
    int max = std::numeric_limits<int>::lowest();
    int min = std::numeric_limits<int>::max();
    int cur = 0;

    if(raw_buffer == nullptr || output_ptr == nullptr)
        return BAD_VALUE;

    createBmpHeader(width, height, &output_ptr);

    for (j = 0; j < height * width; j++) {
        cur = (filter_num == 1) ? af_raw_buffer->y_table[j].y1_avg :\
            af_raw_buffer->y_table[j].y2_avg;

        if (cur > max)
            max = cur;

        if (cur < min)
            min = cur;
    }

    if (max == min) {
        max = (max == 0) ? 1 : min;
        min = 0;
    }

    for (j = 0; j < height; j++) {
        for (i = 0 ; i < width; i++) {
            /* note filter_num =1 is low pass filter, filter_num=2 is high pass filter */
            cur = (filter_num == 1)
                    ? af_raw_buffer->y_table[i + width*j].y1_avg
                    : af_raw_buffer->y_table[i + width*j].y2_avg;
            unsigned char g_channel = (unsigned char)((cur - min) * 255 / (max - min));
            *output_ptr++ = g_channel;
            *output_ptr++ = g_channel;
            *output_ptr++ = g_channel;
        }

        for (i = width % 4; i > 0; i--)
            *output_ptr++ = 0;
    }

    return OK;
}

void StatisticsWorker::createBmpHeader(unsigned int width,
    unsigned int height,
    unsigned char **output_ptr)
{
    BMPFILETYPE type;
    BMPFILEHEADER head;
    BMPINFOHEADER info;

    type.bfType[0] = 'B';
    type.bfType[1] = 'M';

    head.bfSize = 0;
    head.bfReserved1 = head.bfReserved2 = 0;
    head.bfOffBits = sizeof(BMPFILETYPE) + sizeof(BMPFILEHEADER) + sizeof(BMPINFOHEADER);

    info.biSize = sizeof(BMPINFOHEADER);
    info.biWidth = (int32_t)width;
    info.biHeight = ((int32_t)height);
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = 0;
    info.biSizeImage = 0;
    info.biXPelsPerMeter = 0;
    info.biYPelsPerMeter = 0;
    info.biClrUsed = 0;
    info.biClrImportant = 0;

    MEMCPY_S(*output_ptr, sizeof(BMPFILETYPE), &type, sizeof(type));
    *output_ptr += sizeof(BMPFILETYPE);
    MEMCPY_S(*output_ptr, sizeof(BMPFILEHEADER), &head, sizeof(head));
    *output_ptr += sizeof(BMPFILEHEADER);
    MEMCPY_S(*output_ptr, sizeof(BMPINFOHEADER), &info, sizeof(info));
    *output_ptr += sizeof(BMPINFOHEADER);
    LOG2("stat bmp info %dx%d", info.biWidth, info.biHeight);
}

status_t StatisticsWorker::gridToBmp(
    const stats_4a_public_raw_buffer *rgbs_grid_ptr,
    int width, int height,
    unsigned char *output_ptr)
{
    if (rgbs_grid_ptr == nullptr || output_ptr == nullptr)
        return BAD_VALUE;

    unsigned char *rgb_output_ptr = output_ptr;
    unsigned int i, j;
    const awb_public_set_item_t *current_rgbs_block_ptr = rgbs_grid_ptr->awb_raw_buffer.rgb_table;

    createBmpHeader(width, height, &rgb_output_ptr);

    for (j = 0; j < height; j++) {
        for (i = 0 ; i < width; i++) {
            /* one pixel  constitute by R G B */
            *rgb_output_ptr++ = current_rgbs_block_ptr->B_avg;
            *rgb_output_ptr++ = (uint8_t)((current_rgbs_block_ptr->Gr_avg + \
                current_rgbs_block_ptr->Gb_avg) / 2);
            *rgb_output_ptr++ = current_rgbs_block_ptr->R_avg;
            current_rgbs_block_ptr++;
        }

        for (i = ALGIN4(width * 3) - (width * 3); i > 0; i--) {
            *rgb_output_ptr++ = 0;
        }
    }

    return OK;
}

} /* namespace camera2 */
} /* namespace android */
