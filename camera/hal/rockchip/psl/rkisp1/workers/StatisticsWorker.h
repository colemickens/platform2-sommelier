/*
 * Copyright (C) 2017 Intel Corporation.
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

#ifndef PSL_RKISP1_WORKERS_STATISTICSWORKER_H_
#define PSL_RKISP1_WORKERS_STATISTICSWORKER_H_

#include "FrameWorker.h"
#include "CaptureUnit.h"
#include "tasks/ICaptureEventSource.h"
#include <linux/rkisp1-config.h>
#include "Rk3aCore.h"

#define GRID_FILTER_NUM 2

namespace android {
namespace camera2 {

class statusConvertor
{
public:
    statusConvertor();
    ~statusConvertor();

    status_t convertStats(struct rkisp1_stat_buffer* isp_stats, rk_aiq_statistics_input_params* aiq_stats);

private:
    void convertAwbStats(struct cifisp_awb_stat* awb_stats, rk_aiq_awb_measure_result* aiq_awb_stats);
    void convertAeStats(struct cifisp_ae_stat* ae_stats, rk_aiq_aec_measure_result* aiq_ae_stats);
    void convertAfStats(struct cifisp_af_stat* af_stats, rk_aiq_af_meas_stat* aiq_af_stats);
    void convertHistStats(struct cifisp_hist_stat* hist_stats, rk_aiq_aec_measure_result* aiq_hist_stats);

private:
};

class StatisticsWorker: public FrameWorker, public ICaptureEventSource
{
public:
    StatisticsWorker(std::shared_ptr<V4L2VideoNode> node, int cameraId, size_t pipelineDepth);
    virtual ~StatisticsWorker();

    virtual status_t configure(std::shared_ptr<GraphConfig> &config);
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();

private:
    class statusConvertor mConvertor;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_WORKERS_STATISTICSWORKER_H_ */
