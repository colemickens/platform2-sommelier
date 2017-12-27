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

#ifndef PSL_RKISP1_IPC_IPCAIQ_H_
#define PSL_RKISP1_IPC_IPCAIQ_H_
#include <rk_aiq.h>

using namespace android::camera2;
NAMESPACE_DECLARATION {

#define MAX_AIQ_XML_FILE_PATH_SIZE 100
struct aiq_init_params {
    char data[MAX_AIQ_XML_FILE_PATH_SIZE];
    uintptr_t results;
};

struct aiq_deinit_params {
    uintptr_t aiq_handle;
};

struct misc_isp_run_params {
    uintptr_t aiq_handle;

    rk_aiq_misc_isp_input_params base;

    rk_aiq_misc_isp_results results;
};

struct ae_run_params {
    uintptr_t aiq_handle;

    rk_aiq_ae_input_params base;
    rk_aiq_exposure_sensor_descriptor sensor_descriptor;
    rk_aiq_window window;
    long manual_exposure_time_us;
    float manual_analog_gain;
    short manual_iso;
    rk_aiq_ae_manual_limits manual_limits;

    rk_aiq_ae_results results;
};

struct awb_run_params {
    uintptr_t aiq_handle;

    rk_aiq_awb_input_params base;
    rk_aiq_awb_manual_cct_range manual_cct_range;
    rk_aiq_window window;

    rk_aiq_awb_results results;
};

#define MAX_RK_AIQ_VERSION_PARAMS_DATA_SIZE 100
struct rk_aiq_version_params {
    char data[MAX_RK_AIQ_VERSION_PARAMS_DATA_SIZE];
    unsigned int size;
};

struct set_statistics_params_data {
    rk_aiq_statistics_input_params *input;
    rk_aiq_exposure_sensor_descriptor *sensor_desc;
};

struct set_statistics_params {
    uintptr_t aiq_handle;

    set_statistics_params_data base;

    rk_aiq_statistics_input_params input;
    rk_aiq_exposure_sensor_descriptor sensor_desc;

    rk_aiq_ae_results ae_results;
    rk_aiq_awb_results awb_results;
    rk_aiq_misc_isp_results misc_results;
};

class IPCAiq {
public:
    IPCAiq();
    virtual ~IPCAiq();

    // for init
    bool clientFlattenInit(const char* xmlFilePath, aiq_init_params* params);
    bool serverUnflattenInit(const aiq_init_params& inParams, const char** xmlFilePath);

    // for misc
    bool clientFlattenMisc(uintptr_t aiq, const rk_aiq_misc_isp_input_params& inParams, misc_isp_run_params* params);
    bool clientUnflattenMisc(const misc_isp_run_params& params, rk_aiq_misc_isp_results** results);

    // for statistics
    bool clientFlattenStat(uintptr_t aiq, const set_statistics_params_data& inParams, set_statistics_params* params);
    bool serverUnflattenStat(const set_statistics_params& inParams, set_statistics_params_data** params);

    // for ae
    bool clientFlattenAe(uintptr_t aiq, const rk_aiq_ae_input_params& inParams, ae_run_params* params);
    bool clientUnflattenAe(const ae_run_params& params, rk_aiq_ae_results** results);
    bool serverUnflattenAe(const ae_run_params& inParams, rk_aiq_ae_input_params** params);

    // for awb
    bool clientFlattenAwb(uintptr_t aiq, const rk_aiq_awb_input_params& inParams, awb_run_params* params);
    bool clientUnflattenAwb(const awb_run_params& params, rk_aiq_awb_results** results);
    bool serverUnflattenAwb(const awb_run_params& inParams, rk_aiq_awb_input_params** params);

};
} NAMESPACE_DECLARATION_END
#endif // PSL_RKISP1_IPC_IPCAIQ_H_
