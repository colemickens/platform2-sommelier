/*
 * Copyright (C) 2017 Intel Corporation.
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

#ifndef PSL_IPU3_IPC_IPCAIC_H_
#define PSL_IPU3_IPC_IPCAIC_H_
#include "IPU3AICCommon.h"
#include "IPCCommon.h"
#include <KBL_AIC.h>
#include "IPU3ISPPipe.h"
#include "IPCAiq.h"

NAMESPACE_DECLARATION {
typedef struct {
    IPU3AICRuntimeParams base;

    aic_input_frame_parameters_t input_frame_params;
    aic_resolution_config_parameters_t frame_resolution_parameters;
    ia_aiq_output_frame_parameters_t output_frame_params;
    ia_aiq_exposure_parameters exposure_results;
    ia_aiq_hist_weight_grid weight_grid;
    ia_aiq_awb_results awb_results;
    ia_aiq_gbce_results gbce_results;
    pa_run_params_results pa_results;
    sa_run_params_results sa_results;
    ia_rectangle focus_rect;

    // ia_aiq_hist_weight_grid weight_grid;
    unsigned char weights[MAX_SIZE_WEIGHT_GRID];

    // ia_aiq_gbce_results gbce_results;
    float r_gamma_lut[MAX_NUM_GAMMA_LUTS];
    float b_gamma_lut[MAX_NUM_GAMMA_LUTS];
    float g_gamma_lut[MAX_NUM_GAMMA_LUTS];
    float tone_map_lut[MAX_NUM_TOME_MAP_LUTS];
} IPU3AICRuntimeParams_data;

struct Transport {
    unsigned int numPipes;
    ia_binary_data_mod aiqb;
    uintptr_t cmcRemoteHandle;
    IPU3AICRuntimeParams_data input;
    unsigned int dumpAicParameters;
    int test_frameworkDump;
};

#define MAX_IA_AIC_VERSION_PARAMS_DATA_SIZE 100
struct ia_aic_version_params {
    char data[MAX_IA_AIC_VERSION_PARAMS_DATA_SIZE];
    unsigned int size;
};

class IPCAic {
public:
    IPCAic();
    virtual ~IPCAic();

    bool clientFlattenInit(const IPU3AICRuntimeParams& runtimeParams,
                                unsigned int numPipes,
                                const ia_binary_data* aiqb,
                                uintptr_t cmc,
                                unsigned int dumpAicParameters,
                                int testFrameworkDump,
                                Transport* transport);
    bool serverUnflattenInit(const Transport& transport,
                                IPU3AICRuntimeParams** params,
                                ia_binary_data* aiqb,
                                ia_cmc_t** cmc,
                                unsigned int* numPipes,
                                unsigned int* dumpAicParameters,
                                int* test_frameworkDump);

    bool clientFlattenRun(const IPU3AICRuntimeParams& runtimeParams, Transport* transport);
    bool serverUnflattenRun(const Transport& transport, IPU3AICRuntimeParams** params);

    bool clientFlattenReset(const IPU3AICRuntimeParams& runtimeParams, Transport* transport);
    bool serverUnflattenReset(const Transport& transport, IPU3AICRuntimeParams** params);

private:
    bool flattenIPU3AICRuntimeParams(const IPU3AICRuntimeParams& params, IPU3AICRuntimeParams_data* input);
    bool unflattenIPU3AICRuntimeParams(IPU3AICRuntimeParams_data* input);
};
} NAMESPACE_DECLARATION_END
#endif // PSL_IPU3_IPC_IPCAIC_H_
