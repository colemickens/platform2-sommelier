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

#define LOG_TAG "StatisticsWorker"

#include <linux/rkisp1-config.h>
#include "StatisticsWorker.h"
#include "LogHelper.h"
#include "NodeTypes.h"

namespace android {
namespace camera2 {

StatisticsWorker::StatisticsWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId, size_t pipelineDepth) :
        FrameWorker(node, cameraId, pipelineDepth, "StatisticsWorker")
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
    status_t ret = OK;

    FrameInfo frame;
    frame.format = V4L2_META_FMT_RK_ISP1_STAT_3A;
    ret = setWorkerDeviceFormat(frame);
    if (ret != OK)
        return ret;

    ret = setWorkerDeviceBuffers(getDefaultMemoryType(IMGU_NODE_STAT));
    if (ret != OK)
        return ret;

    ret = allocateWorkerBuffers();
    if (ret != OK)
        return ret;

    if (mCameraBuffers[0]->size() < sizeof(rkisp1_stat_buffer)) {
        LOGE("Stats buffer is not big enough");
        return UNKNOWN_ERROR;
    }

    mIndex = 0;

    return ret;
}

status_t StatisticsWorker::prepareRun(std::shared_ptr<DeviceMessage> msg)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    status_t status = OK;

    if (mDevError)
        return OK;

    mMsg = msg;

    status |= mNode->putFrame(mBuffers[mIndex]);

    if (status != OK) {
        LOGE("Failed to queue buffer to statistics device");
        return status;
    }

    mIndex = (mIndex + 1) % mPipelineDepth;

    return OK;
}

static void dumpAE(struct cifisp_ae_stat& ae) {
    LOG2("AecStatDump:exp_mean(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d), bls_val(%d,%d,%d,%d)",
         ae.exp_mean[0],ae.exp_mean[1],ae.exp_mean[2],ae.exp_mean[3],ae.exp_mean[4],ae.exp_mean[5],ae.exp_mean[6],ae.exp_mean[7],
         ae.exp_mean[8],ae.exp_mean[9],ae.exp_mean[10],ae.exp_mean[11],ae.exp_mean[12],ae.exp_mean[13],ae.exp_mean[14],ae.exp_mean[15],
         ae.exp_mean[16],ae.exp_mean[17],ae.exp_mean[18],ae.exp_mean[19],ae.exp_mean[20],ae.exp_mean[21],ae.exp_mean[22],ae.exp_mean[23],ae.exp_mean[24],
         ae.bls_val.meas_r, ae.bls_val.meas_gr, ae.bls_val.meas_gb, ae.bls_val.meas_b);
}

static void dumpAWB(struct cifisp_awb_stat& awb) {
    LOG2("AwbStatDump:awb:mean:cnt(%d), awb:mean:y_or_g(%d), awb:mean:cb_or_b(%d), awb:mean:cr_or_r(%d)",
         awb.awb_mean[0].cnt,
         awb.awb_mean[0].mean_y_or_g,
         awb.awb_mean[0].mean_cb_or_b,
         awb.awb_mean[0].mean_cr_or_r);
}

static void dumpAF(struct cifisp_af_stat& af) {
    LOG2("AfStatDump:window[1] (%d, %d), window[2] (%d, %d), window[3] (%d, %d) ",
         af.window[0].sum, af.window[0].lum,
         af.window[1].sum, af.window[1].lum,
         af.window[2].sum, af.window[2].lum);
}

static void dumpHIST(struct cifisp_hist_stat& hist) {
    for (int i = 0; i < 2; i++) {
        LOG2("HistStatDump:hist_bins[%d-%d]: %d, %d, %d, %d, %d, %d, %d, %d",
             i*8, i*8+7, hist.hist_bins[i*8], hist.hist_bins[i*8+1], hist.hist_bins[i*8+2], hist.hist_bins[i*8+3], hist.hist_bins[i*8+4], hist.hist_bins[i*8+5], hist.hist_bins[i*8+6], hist.hist_bins[i*8+7]);
    }
}

static void dumpStats(struct rkisp1_stat_buffer* stats) {
    LOG2("%s:%d: frame_id(%d), meas_type(%d)", __func__, __LINE__, stats->frame_id, stats->meas_type);
    dumpAE(stats->params.ae);
    dumpAWB(stats->params.awb);
    dumpAF(stats->params.af);
    dumpHIST(stats->params.hist);
}

status_t StatisticsWorker::run()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    if (mDevError)
        return OK;

    if (!mMsg) {
        LOGE("Message is not set - Fix the bug");
        return UNKNOWN_ERROR;
    }

    #define DUMP_INTERVAL 10
    status_t status = OK;

    V4L2BufferInfo outBuf;

    status = mNode->grabFrame(&outBuf);
    int index = outBuf.vbuffer.index();
    rkisp1_stat_buffer* isp_stats = (rkisp1_stat_buffer*)mCameraBuffers[index]->data();

    if (status < 0) {
        LOGE("Failed to dequeue buffer from statistics device");
        return status;
    }

    auto stats = std::make_shared<rk_aiq_statistics_input_params>();
    if (stats.get() == nullptr) {
        LOGE("Not enough memory to allocate stats");
        return NO_MEMORY;
    }
    memset(stats.get(), 0, sizeof(rk_aiq_statistics_input_params));
    dumpStats(isp_stats);
    status = mConvertor.convertStats(isp_stats, stats.get());
    if (status != OK) {
        return UNKNOWN_ERROR;
    }

    stats->frame_id = outBuf.vbuffer.sequence();
    stats->frame_timestamp = (outBuf.vbuffer.timestamp().tv_sec * 1000000) + outBuf.vbuffer.timestamp().tv_usec;
    LOG2("%s:%d: frame_id(%llu)", __func__, __LINE__, stats->frame_id);

    ICaptureEventListener::CaptureMessage outMsg;
    outMsg.id = ICaptureEventListener::CAPTURE_MESSAGE_ID_EVENT;
    outMsg.data.event.type = ICaptureEventListener::CAPTURE_EVENT_2A_STATISTICS;
    outMsg.data.event.stats = stats;

    notifyListeners(&outMsg);

    return OK;
}

status_t StatisticsWorker::postRun()
{
    mMsg = nullptr;
    return OK;
}

statusConvertor::statusConvertor()
{
}

statusConvertor::~statusConvertor()
{
}

void statusConvertor::convertAwbStats(struct cifisp_awb_stat* awb_stats, rk_aiq_awb_measure_result* aiq_awb_stats)
{
    for (int i = 0; i < CIFISP_AWB_MAX_GRID; i++) {
        aiq_awb_stats->awb_meas[i].num_white_pixel = awb_stats->awb_mean[i].cnt;
        aiq_awb_stats->awb_meas[i].mean_y__g = awb_stats->awb_mean[i].mean_y_or_g;
        aiq_awb_stats->awb_meas[i].mean_cb__b = awb_stats->awb_mean[i].mean_cb_or_b;
        aiq_awb_stats->awb_meas[i].mean_cr__r = awb_stats->awb_mean[i].mean_cr_or_r;
    }
}

void statusConvertor::convertAeStats(struct cifisp_ae_stat* ae_stats, rk_aiq_aec_measure_result* aiq_ae_stats)
{
    for (int i = 0; i < CIFISP_AE_MEAN_MAX; i++) {
        aiq_ae_stats->exp_mean[i] = ae_stats->exp_mean[i];
    }
}

void statusConvertor::convertAfStats(struct cifisp_af_stat* af_stats, rk_aiq_af_meas_stat* aiq_af_stats)
{
    for (int i = 0; i < CIFISP_AFM_MAX_WINDOWS; i++) {
        aiq_af_stats->window[i].lum = af_stats->window[i].lum;
        aiq_af_stats->window[i].sum = af_stats->window[i].sum;
    }
}

void statusConvertor::convertHistStats(struct cifisp_hist_stat* hist_stats, rk_aiq_aec_measure_result* aiq_hist_stats)
{
    for (int i = 0; i < CIFISP_HIST_BIN_N_MAX; i++) {
        aiq_hist_stats->hist_bin[i] = hist_stats->hist_bins[i];
    }
}

status_t statusConvertor::convertStats(struct rkisp1_stat_buffer* isp_stats, rk_aiq_statistics_input_params* aiq_stats)
{
    if (CC_UNLIKELY(isp_stats == NULL || aiq_stats == NULL))
    {
        LOGE("fuxiang %s:%d: status buffer or aiq_stats is NULL, is a bug , fix me", __func__, __LINE__);
        return UNKNOWN_ERROR;
    }
    convertAwbStats(&isp_stats->params.awb, &aiq_stats->awb_stats);
    convertAeStats(&isp_stats->params.ae, &aiq_stats->aec_stats);
    convertAfStats(&isp_stats->params.af, &aiq_stats->af_stats);
    convertHistStats(&isp_stats->params.hist, &aiq_stats->aec_stats);
    return OK;
}

} /* namespace camera2 */
} /* namespace android */
