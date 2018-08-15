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

#ifndef PSL_IPU3_IPC_IPCAIQ_H_
#define PSL_IPU3_IPC_IPCAIQ_H_
#include <ia_aiq.h>
#include "IPCFaceEngine.h"

namespace android {
namespace camera2 {
struct aiq_init_params {
    unsigned int aiqb_size;
    unsigned int nvm_size;
    unsigned int aiqd_size;
    unsigned int stats_max_width;
    unsigned int stats_max_height;
    unsigned int max_num_stats_in;
    uintptr_t ia_mkn;
    uintptr_t cmcRemoteHandle;
    uintptr_t results;
};

struct aiq_deinit_params {
    uintptr_t aiq_handle;
};

struct af_run_params {
    uintptr_t aiq_handle;

    ia_aiq_af_input_params base;
    ia_rectangle focus_rect;
    ia_aiq_manual_focus_parameters manual_focus_parameters;

    ia_aiq_af_results results;
};

#define MAX_NUM_GAMMA_LUTS 1024
#define MAX_NUM_TOME_MAP_LUTS 1024
struct gbce_run_params {
    uintptr_t aiq_handle;

    ia_aiq_gbce_input_params base;

    ia_aiq_gbce_results resBase;
    float r_gamma_lut[MAX_NUM_GAMMA_LUTS];
    float b_gamma_lut[MAX_NUM_GAMMA_LUTS];
    float g_gamma_lut[MAX_NUM_GAMMA_LUTS];
    float tone_map_lut[MAX_NUM_TOME_MAP_LUTS];
};

#define MAX_NUM_EXPOSURES 1
#define MAX_NUM_FLASHES 1
#define MAX_NUM_OF_EXPOSURE_PLANS 1
#define MAX_SIZE_WEIGHT_GRID (128 * 128)
struct ae_run_params_results {
    ia_aiq_ae_results base;

    ia_aiq_ae_exposure_result exposures[MAX_NUM_EXPOSURES];
    ia_aiq_hist_weight_grid weight_grid;
    ia_aiq_flash_parameters flashes[MAX_NUM_FLASHES];
    ia_aiq_aperture_control aperture_control;

    // the below is in ia_aiq_ae_exposure_result exposures[MAX_NUM_EXPOSURES];
    ia_aiq_exposure_parameters exposure;
    ia_aiq_exposure_sensor_parameters sensor_exposure;
    unsigned int exposure_plan_ids[MAX_NUM_OF_EXPOSURE_PLANS];

    // the below is in ia_aiq_hist_weight_grid weight_grid;
    unsigned char weights[MAX_SIZE_WEIGHT_GRID];
};

struct ae_run_params {
    uintptr_t aiq_handle;

    ia_aiq_ae_input_params base;
    ia_aiq_exposure_sensor_descriptor sensor_descriptor;
    ia_rectangle exposure_window;
    ia_coordinate exposure_coordinate;
    long manual_exposure_time_us;
    float manual_analog_gain;
    short manual_iso;
    ia_aiq_ae_features aec_features;
    ia_aiq_ae_manual_limits manual_limits;

    ae_run_params_results res;
};

struct awb_run_params {
    uintptr_t aiq_handle;

    ia_aiq_awb_input_params base;
    ia_aiq_awb_manual_cct_range manual_cct_range;
    ia_coordinate manual_white_coordinate;

    ia_aiq_awb_results results;
};

#define MAX_NUM_LUTS 128
#define MAX_SECTOR_COUNT 128
#define MAX_IR_WIDTH 128
#define MAX_IR_HEIGHT 128
#define MAX_NUM_IR_BLOCKS (MAX_IR_WIDTH * MAX_IR_HEIGHT)
struct pa_run_params_results {
    ia_aiq_pa_results base;

    ia_aiq_advanced_ccm_t preferred_acm;
    ia_aiq_ir_weight_t ir_weight;

    // for ia_aiq_color_channels_lut linearization
    float gr[MAX_NUM_LUTS];
    float r[MAX_NUM_LUTS];
    float b[MAX_NUM_LUTS];
    float gb[MAX_NUM_LUTS];

    // for ia_aiq_advanced_ccm_t *preferred_acm
    unsigned int hue_of_sectors[MAX_SECTOR_COUNT];
    float advanced_color_conversion_matrices[MAX_SECTOR_COUNT][3][3];

    // for ia_aiq_ir_weight_t *ir_weight
    unsigned short ir_weight_grid_R[MAX_NUM_IR_BLOCKS];
    unsigned short ir_weight_grid_G[MAX_NUM_IR_BLOCKS];
    unsigned short ir_weight_grid_B[MAX_NUM_IR_BLOCKS];
};

struct ia_face_state_data {
    ia_face_state base;
    ia_face faces[MAX_FACES_DETECTABLE];
};

struct pa_run_params {
    uintptr_t aiq_handle;

    ia_aiq_pa_input_params base;
    ia_aiq_awb_results awb_results;
    ia_aiq_exposure_parameters exposure_params;
    ia_aiq_color_channels color_gains;

    pa_run_params_results res;
};

#define LSC_TABLE_MAX_WIDTH 128
#define LSC_TABLE_MAX_HEIGHT 128
#define LSC_TABLE_MAX_SIZE (LSC_TABLE_MAX_WIDTH * LSC_TABLE_MAX_HEIGHT)
struct sa_run_params_results {
    ia_aiq_sa_results base;

    float channel_gr[LSC_TABLE_MAX_SIZE];
    float channel_r[LSC_TABLE_MAX_SIZE];
    float channel_b[LSC_TABLE_MAX_SIZE];
    float channel_gb[LSC_TABLE_MAX_SIZE];
};

struct sa_run_params {
    uintptr_t aiq_handle;

    ia_aiq_sa_input_params base;
    ia_aiq_frame_params sensor_frame_params;
    ia_aiq_awb_results awb_results;

    sa_run_params_results res;
};

#define MAX_IA_BINARY_DATA_PARAMS_SIZE 500000
struct ia_binary_data_params
{
    uintptr_t aiq_handle;
    uint8_t data[MAX_IA_BINARY_DATA_PARAMS_SIZE];
    unsigned int size;
};

#define MAX_IA_AIQ_VERSION_PARAMS_DATA_SIZE 100
struct ia_aiq_version_params {
    char data[MAX_IA_AIQ_VERSION_PARAMS_DATA_SIZE];
    unsigned int size;
};

#define MAX_WIDTH 128
#define MAX_HEIGHT 128
#define MAX_NUM_BLOCKS (MAX_WIDTH * MAX_HEIGHT)
struct ia_aiq_rgbs_grid_data {
    ia_aiq_rgbs_grid base;

    rgbs_grid_block blocks_ptr[MAX_NUM_BLOCKS];
};

struct ia_aiq_hdr_rgbs_grid_data {
    ia_aiq_hdr_rgbs_grid base;

    hdr_rgbs_grid_block blocks_ptr[MAX_NUM_BLOCKS];
};

#define MAX_AF_GRID_WIDTH 80
#define MAX_AF_GRID_HEIGHT 60
#define MAX_AF_GRID_SIZE (MAX_AF_GRID_HEIGHT * MAX_AF_GRID_WIDTH)
struct ia_aiq_af_grid_data {
    ia_aiq_af_grid base;

    int filter_response_1[MAX_AF_GRID_SIZE];
    int filter_response_2[MAX_AF_GRID_SIZE];
};

#define MAX_DEPTH_GRID_WIDHT 128
#define MAX_DEPTH_GRID_HEIGHT 128
#define MAX_DEPTH_GRID_SIZE (MAX_DEPTH_GRID_WIDHT * MAX_DEPTH_GRID_HEIGHT)
struct ia_aiq_depth_grid_data {
    ia_aiq_depth_grid base;

    ia_rectangle grid_rect[MAX_DEPTH_GRID_SIZE];
    int depth_data[MAX_DEPTH_GRID_SIZE];
    unsigned char confidence[MAX_DEPTH_GRID_SIZE];
};

#define MAX_NUMBER_OF_GRIDS 1
#define MAX_NUMBER_OF_AF_GRIDS 1
#define MAX_NUMBER_OF_HISTROGRAMS 1
#define MAX_NUMBER_OF_DEPTH_GRIDS 1
struct set_statistics_params_data {
    ia_aiq_statistics_input_params base;

    ae_run_params_results frame_ae_parameters;

    ia_aiq_af_results frame_af_parameters;

    const ia_aiq_rgbs_grid* rgbs_grids_array[MAX_NUMBER_OF_GRIDS];
    ia_aiq_rgbs_grid_data rgbs_grids[MAX_NUMBER_OF_GRIDS];

    ia_aiq_hdr_rgbs_grid_data hdr_rgbs_grid;

    const ia_aiq_af_grid* af_grids_array[MAX_NUMBER_OF_AF_GRIDS];
    ia_aiq_af_grid_data af_grids[MAX_NUMBER_OF_AF_GRIDS];

    pa_run_params_results frame_pa_parameters;

    ia_face_state_data faces;

    ia_aiq_awb_results awb_results;

    sa_run_params_results frame_sa_parameters;

    const ia_aiq_depth_grid* depth_grids_array[MAX_NUMBER_OF_DEPTH_GRIDS];
    ia_aiq_depth_grid_data depth_grids[MAX_NUMBER_OF_DEPTH_GRIDS];
};

struct set_statistics_params {
    uintptr_t ia_aiq;
    set_statistics_params_data input;
};

class IPCAiq {
public:
    IPCAiq();
    virtual ~IPCAiq();

    // for init
    bool clientFlattenInit(const ia_binary_data* aiqb_data, unsigned int aiqbSize,
                const ia_binary_data* nvm_data, unsigned int nvmSize,
                const ia_binary_data* aiqd_data,unsigned int aiqdSize,
                unsigned int statsMaxWidth,
                unsigned int statsMaxHeight,
                unsigned int maxNumStatsIn,
                uintptr_t mkn,
                uintptr_t cmc,
                uint8_t* pData, unsigned int size);
    bool serverUnflattenInit(void* pData, int dataSize, ia_binary_data* aiqbData, ia_binary_data* nvmData, ia_binary_data* aiqdData);

    // for af
    bool clientFlattenAf(uintptr_t aiq, const ia_aiq_af_input_params& inParams, af_run_params* params);
    bool clientUnflattenAf(const af_run_params& params, ia_aiq_af_results** results);
    bool serverUnflattenAf(const af_run_params& inParams, ia_aiq_af_input_params** params);
    bool serverFlattenAf(const ia_aiq_af_results& afResults, af_run_params* params);

    // for gbce
    bool clientFlattenGbce(uintptr_t aiq, const ia_aiq_gbce_input_params& inParams, gbce_run_params* params);
    bool clientUnflattenGbce(const gbce_run_params& params, ia_aiq_gbce_results** results);
    bool serverFlattenGbce(const ia_aiq_gbce_results& gbceResults, gbce_run_params* params);

    // for statistics
    bool clientFlattenStat(uintptr_t aiq, const ia_aiq_statistics_input_params& inParams, set_statistics_params* params);
    bool serverUnflattenStat(const set_statistics_params& inParams, ia_aiq_statistics_input_params** params);

    // for ae
    bool clientFlattenAe(uintptr_t aiq, const ia_aiq_ae_input_params& inParams, ae_run_params* params);
    bool clientUnflattenAe(const ae_run_params& params, ia_aiq_ae_results** results);
    bool serverUnflattenAe(const ae_run_params& inParams, ia_aiq_ae_input_params** params);
    bool serverFlattenAe(const ia_aiq_ae_results& aeResults, ae_run_params* params);

    // for awb
    bool clientFlattenAwb(uintptr_t aiq, const ia_aiq_awb_input_params& inParams, awb_run_params* params);
    bool clientUnflattenAwb(const awb_run_params& inParams, ia_aiq_awb_results** results);
    bool serverUnflattenAwb(const awb_run_params& inParams, ia_aiq_awb_input_params** params);
    bool serverFlattenAwb(const ia_aiq_awb_results& awbResults, awb_run_params* params);

    // for pa
    bool clientFlattenPa(uintptr_t aiq, const ia_aiq_pa_input_params& inParams, pa_run_params* params);
    bool clientUnflattenPa(const pa_run_params& params, ia_aiq_pa_results** results);
    bool serverUnflattenPa(const pa_run_params& inParams, ia_aiq_pa_input_params** params);
    bool serverFlattenPa(const ia_aiq_pa_results& paResults, pa_run_params* params);

    // for sa
    bool clientFlattenSa(uintptr_t aiq, const ia_aiq_sa_input_params& inParams, sa_run_params* params);
    bool clientUnflattenSa(const sa_run_params& params, ia_aiq_sa_results** results);
    bool serverUnflattenSa(const sa_run_params& inParams, ia_aiq_sa_input_params** params);
    bool serverFlattenSa(const ia_aiq_sa_results& saResults, sa_run_params* params);

    static bool flattenSaResults(const ia_aiq_sa_results& saResults, sa_run_params_results* res);
    static bool unflattenSaResults(sa_run_params_results* res);

    static bool flattenPaResults(const ia_aiq_pa_results& paResults, pa_run_params_results* res);
    static bool unflattenPaResults(pa_run_params_results* res);

private:
    bool flattenAeResults(const ia_aiq_ae_results& aeResults, ae_run_params_results* res);
    bool unflattenAeResults(ae_run_params_results* res);

};
} /* namespace camera2 */
} /* namespace android */
#endif // PSL_IPU3_IPC_IPCAIQ_H_
