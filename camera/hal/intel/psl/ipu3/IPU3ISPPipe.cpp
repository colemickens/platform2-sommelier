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

#define LOG_TAG "IPU3ISPPipe"

#include "IPU3ISPPipe.h"
#include "LogHelper.h"
#include <string.h>

namespace android {
namespace camera2 {

IPU3ISPPipe::IPU3ISPPipe()
{
    CLEAR(mAicOutput);
    CLEAR(mAicConfig);
}

IPU3ISPPipe::~IPU3ISPPipe()
{
}

void IPU3ISPPipe::SetPipeConfig(const aic_output_t pipe_config)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL2);

    mAicOutput = pipe_config;

    if (mAicOutput.lin_2500_config)
        memcpy(&mAicConfig.lin_2500_config, mAicOutput.lin_2500_config, sizeof(ia_css_2500_lin_kernel_config));
    if (mAicOutput.obgrid_2500_config)
        memcpy(&mAicConfig.obgrid_2500_config, mAicOutput.obgrid_2500_config, sizeof(ia_css_2500_obgrid_kernel_config));
    if (mAicOutput.bnr_2500_config)
        memcpy(&mAicConfig.bnr_2500_config, mAicOutput.bnr_2500_config, sizeof(ia_css_2500_bnr_kernel_config));
    if (mAicOutput.shd_2500_config)
        memcpy(&mAicConfig.shd_2500_config, mAicOutput.shd_2500_config, sizeof(ia_css_2500_shd_kernel_config));
    if (mAicOutput.dm_2500_config)
        memcpy(&mAicConfig.dm_2500_config, mAicOutput.dm_2500_config, sizeof(ia_css_2500_dm_kernel_config));
    if (mAicOutput.rgbpp_2500_config)
        memcpy(&mAicConfig.rgbpp_2500_config, mAicOutput.rgbpp_2500_config, sizeof(ia_css_2500_rgbpp_kernel_config));
    if (mAicOutput.yuvp1_2500_config)
        memcpy(&mAicConfig.yuvp1_2500_config, mAicOutput.yuvp1_2500_config, sizeof(ia_css_2500_yuvp1_b0_kernel_config));
    if (mAicOutput.yuvp1_c0_2500_config)
        memcpy(&mAicConfig.yuvp1_c0_2500_config, mAicOutput.yuvp1_c0_2500_config, sizeof(ia_css_2500_yuvp1_c0_kernel_config));
    if (mAicOutput.yuvp2_2500_config)
        memcpy(&mAicConfig.yuvp2_2500_config, mAicOutput.yuvp2_2500_config, sizeof(ia_css_2500_yuvp2_kernel_config));
    if (mAicOutput.tnr3_2500_config)
        memcpy(&mAicConfig.tnr3_2500_config, mAicOutput.tnr3_2500_config, sizeof(ia_css_tnr3_kernel_config));
    if (mAicOutput.dpc_2500_config)
        memcpy(&mAicConfig.dpc_2500_config, mAicOutput.dpc_2500_config, sizeof(ia_css_2500_dpc_kernel_config));
    if (mAicOutput.awb_2500_config)
        memcpy(&mAicConfig.awb_2500_config, mAicOutput.awb_2500_config, sizeof(ia_css_2500_awb_kernel_config));
    if (mAicOutput.awb_fr_2500_config)
        memcpy(&mAicConfig.awb_fr_2500_config, mAicOutput.awb_fr_2500_config, sizeof(ia_css_2500_awb_fr_kernel_config));
    if (mAicOutput.anr_2500_config)
        memcpy(&mAicConfig.anr_2500_config, mAicOutput.anr_2500_config, sizeof(ia_css_2500_anr_kernel_config));
    if (mAicOutput.af_2500_config)
        memcpy(&mAicConfig.af_2500_config, mAicOutput.af_2500_config, sizeof(ia_css_2500_af_kernel_config));
    if (mAicOutput.ae_2500_config)
        memcpy(&mAicConfig.ae_2500_config, mAicOutput.ae_2500_config, sizeof(ia_css_2500_ae_kernel_config));
    if (mAicOutput.xnr_2500_config)
        memcpy(&mAicConfig.xnr_2500_config, mAicOutput.xnr_2500_config, sizeof(ia_css_xnr3_config));

    if (mAicOutput.rgbir_2500_config)
        memcpy(&mAicConfig.rgbir_2500_config, mAicOutput.rgbir_2500_config, sizeof(ia_css_2500_rgbir_kernel_config));
}

void IPU3ISPPipe::dump()
{
    if (mAicOutput.ae_2500_config) {
        LOGD("mAicOutput.ae_2500_config->ae.ae_grid_config.ae_en %d", mAicOutput.ae_2500_config->ae.ae_grid_config.ae_en);
        LOGD("mAicOutput.ae_2500_config->ae.ae_grid_config.block_height %d", mAicOutput.ae_2500_config->ae.ae_grid_config.block_height);
    }
    if (mAicOutput.af_2500_config) {
        LOGD("mAicOutput.af_2500_config->af.grid.grid_height %d", mAicOutput.af_2500_config->af.grid.grid_height);
        LOGD("mAicOutput.af_2500_config->af.grid.grid_width %d", mAicOutput.af_2500_config->af.grid.grid_width);
    }
    if (mAicOutput.anr_2500_config) {
            LOGD("mAicOutput.anr_2500_config %d", mAicOutput.anr_2500_config);
    }
    if (mAicOutput.awb_2500_config) {
            LOGD("mAicOutput.awb_2500_config->awb.grid.grid_block_height: %d", mAicOutput.awb_2500_config->awb.grid.grid_block_height);
            LOGD("mAicOutput.awb_2500_config->awb.grid.grid_block_width: %d", mAicOutput.awb_2500_config->awb.grid.grid_block_width);
    }
    if (mAicOutput.awb_fr_2500_config) {
            LOGD("mAicOutput.awb_fr_2500_config %d", mAicOutput.awb_fr_2500_config);
    }
    if (mAicOutput.bnr_2500_config) {
            LOGD("mAicOutput.bnr_2500_config %d", mAicOutput.bnr_2500_config);
    }
    if (mAicOutput.dm_2500_config) {
            LOGD("mAicOutput.dm_2500_config %d", mAicOutput.dm_2500_config);
    }
    if (mAicOutput.dpc_2500_config) {
            LOGD("mAicOutput.dpc_2500_config %d", mAicOutput.dpc_2500_config);
    }
    if (mAicOutput.lin_2500_config) {
            LOGD("mAicOutput.lin_2500_config %d", mAicOutput.lin_2500_config);
    }
    if (mAicOutput.obgrid_2500_config) {
            LOGD("mAicOutput.obgrid_2500_config %d", mAicOutput.obgrid_2500_config);
    }
    if (mAicOutput.rgbir_2500_config) {
            LOGD("mAicOutput.rgbir_2500_config %d", mAicOutput.rgbir_2500_config);
    }
    if (mAicOutput.rgbpp_2500_config) {
            LOGD("mAicOutput.rgbpp_2500_config %d", mAicOutput.rgbpp_2500_config);
    }
    if (mAicOutput.shd_2500_config) {
            LOGD("mAicOutput.shd_2500_config %d", mAicOutput.shd_2500_config);
    }
    if (mAicOutput.tnr3_2500_config) {
            LOGD("mAicOutput.tnr3_2500_config %d", mAicOutput.tnr3_2500_config);
    }
    if (mAicOutput.xnr_2500_config) {
            LOGD("mAicOutput.xnr_2500_config %d", mAicOutput.xnr_2500_config);
    }
    if (mAicOutput.yuvp1_2500_config) {
            LOGD("mAicOutput.yuvp1_2500_config %d", mAicOutput.yuvp1_2500_config);
    }
    if (mAicOutput.yuvp1_c0_2500_config) {
            LOGD("mAicOutput.yuvp1_c0_2500_config %d", mAicOutput.yuvp1_c0_2500_config);
    }
    if (mAicOutput.yuvp2_2500_config) {
            LOGD("mAicOutput.yuvp2_2500_config %d", mAicOutput.yuvp2_2500_config);
    }
}

const ia_aiq_rgbs_grid* IPU3ISPPipe::GetAWBStats()
{
    return nullptr;
}

const ia_aiq_af_grid* IPU3ISPPipe::GetAFStats()
{
    return nullptr;
}

const ia_aiq_histogram* IPU3ISPPipe::GetAEStats()
{
    return nullptr;
}

aic_config* IPU3ISPPipe::GetAicConfig()
{
    return &mAicConfig;
}



} /* namespace camera2 */
} /* namespace android */
