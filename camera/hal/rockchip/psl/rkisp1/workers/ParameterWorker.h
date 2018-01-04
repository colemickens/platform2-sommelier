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

#ifndef PSL_RKISP1_WORKERS_PARAMETERWORKER_H_
#define PSL_RKISP1_WORKERS_PARAMETERWORKER_H_

#include <mutex>
#include "FrameWorker.h"
#include <linux/rkisp1-config.h>
#include "Rk3aCore.h"
#include "rkisp1_regs.h"

namespace android {
namespace camera2 {

class paramConvertor
{
public:
    paramConvertor();
    ~paramConvertor();

    bool areNewParams(struct AiqResults* aiqResults);
    status_t convertParams(struct rkisp1_isp_params_cfg* isp_cfg, struct AiqResults* aiqResults);

private:
    void convertDpcc(struct cifisp_dpcc_config* dpcc_config, rk_aiq_dpcc_config* aiq_dpcc_config);
    void convertBls(struct cifisp_bls_config* bls_config, rk_aiq_bls_config* aiq_bls_config);
    void convertSdg(struct cifisp_sdg_config* sdg_config, rk_aiq_sdg_config* aiq_sdg_config);
    void convertHst(struct cifisp_hst_config* hst_config, rk_aiq_hist_config* aiq_hst_config);
    void convertLsc(struct cifisp_lsc_config* lsc_config, rk_aiq_lsc_config* aiq_lsc_config);
    void convertAwbGain(struct cifisp_awb_gain_config* awbGain_config, rk_aiq_awb_gain_config* aiq_awbGain_config);
    void convertFlt(struct cifisp_flt_config* flt_config, rk_aiq_flt_config* aiq_flt_config);
    void convertBdm(struct cifisp_bdm_config* bdm_config, rk_aiq_bdm_config* aiq_bdm_config);
    void convertCtk(struct cifisp_ctk_config* ctk_config, rk_aiq_ctk_config* aiq_ctk_config);
    void convertGoc(struct cifisp_goc_config* goc_config, rk_aiq_goc_config* aiq_goc_config);
    void convertCproc(struct cifisp_cproc_config* cproc_config, rk_aiq_cproc_config* aiq_cproc_config);
    void convertAfc(struct cifisp_afc_config* afc_config, rk_aiq_afc_cfg* aiq_afc_config);
    void convertAwb(struct cifisp_awb_meas_config* awb_config, rk_aiq_awb_measure_config* aiq_awb_config);
    void convertIe(struct cifisp_ie_config* ie_config, rk_aiq_ie_config* aiq_ie_config);
    void convertAec(struct cifisp_aec_config* aec_config, rk_aiq_aec_config* aiq_aec_config);
    void convertDpf(struct cifisp_dpf_config* dpf_config, rk_aiq_dpf_config* aiq_dpf_config);
    void convertDpfStrength(struct cifisp_dpf_strength_config* dpfStrength_config, rk_aiq_dpf_strength_config* aiq_dpfStrength_config);

private:
    struct rkisp1_isp_params_cfg* mpIspParams;
    struct AiqResults mLastAiqResults;
};

class ParameterWorker: public FrameWorker
{
public:
    ParameterWorker(std::shared_ptr<V4L2VideoNode> node, const StreamConfig& activeStreams, int cameraId, size_t pipelineDepth);
    virtual ~ParameterWorker();

    virtual status_t configure(std::shared_ptr<GraphConfig> &config);
    status_t prepareRun(std::shared_ptr<DeviceMessage> msg);
    status_t run();
    status_t postRun();

private:
    status_t grabFrame();

    class paramConvertor mConvertor;

    bool mSeenFirstParams;

    int mCurSeq;
    int mLastSeq;
    CameraMetadata mCurMetadata;
    CameraMetadata mLastMetadata;

    // Cached metadatas
    std::vector<CameraMetadata> mMetadatas;
};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_RKISP1_WORKERS_PARAMETERWORKER_H_ */
