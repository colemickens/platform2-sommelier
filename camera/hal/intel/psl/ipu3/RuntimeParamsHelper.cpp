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

#define LOG_TAG "AicLibrary"

#include <string.h>
#include "RuntimeParamsHelper.h"
#include "IPU3ISPPipe.h"
#include "LogHelper.h"
#include <ia_aiq_types.h>

namespace android {
namespace camera2 {

RuntimeParamsHelper::RuntimeParamsHelper() {
}

RuntimeParamsHelper::~RuntimeParamsHelper() {
}

void
RuntimeParamsHelper::copyPaResults(IPU3AICRuntimeParams &to, ia_aiq_pa_results &from)
{
    ia_aiq_pa_results *pa_results;
    pa_results = (ia_aiq_pa_results*) to.pa_results;
    pa_results->black_level = from.black_level;
    pa_results->brightness_level = from.brightness_level;
    memcpy(pa_results->color_conversion_matrix, from.color_conversion_matrix,
           sizeof(pa_results->color_conversion_matrix));
    pa_results->color_gains = from.color_gains;
    if (from.ir_weight && pa_results->ir_weight) {
        int size = pa_results->ir_weight->height * pa_results->ir_weight->width;
        memcpy(pa_results->ir_weight->ir_weight_grid_B, from.ir_weight->ir_weight_grid_B,
                sizeof(*pa_results->ir_weight->ir_weight_grid_B)*size);
        memcpy(pa_results->ir_weight->ir_weight_grid_G, from.ir_weight->ir_weight_grid_G,
                sizeof(*pa_results->ir_weight->ir_weight_grid_G)*size);
        memcpy(pa_results->ir_weight->ir_weight_grid_R, from.ir_weight->ir_weight_grid_R,
                sizeof(*pa_results->ir_weight->ir_weight_grid_R)*size);
    }
    memcpy(pa_results->linearization.b, from.linearization.b,
            sizeof(*pa_results->linearization.b) * pa_results->linearization.size);
    memcpy(pa_results->linearization.gb, from.linearization.gb,
            sizeof(*pa_results->linearization.gb) * pa_results->linearization.size);
    memcpy(pa_results->linearization.gr, from.linearization.gr,
            sizeof(*pa_results->linearization.gr) * pa_results->linearization.size);
    memcpy(pa_results->linearization.r, from.linearization.r,
            sizeof(*pa_results->linearization.r) * pa_results->linearization.size);
    if (from.preferred_acm && pa_results->preferred_acm) {
        memcpy(pa_results->preferred_acm->advanced_color_conversion_matrices, from.preferred_acm->advanced_color_conversion_matrices,
               sizeof(*pa_results->preferred_acm->advanced_color_conversion_matrices) * from.preferred_acm->sector_count);
        memcpy(pa_results->preferred_acm->hue_of_sectors, from.preferred_acm->hue_of_sectors,
                sizeof(*pa_results->preferred_acm->hue_of_sectors) * pa_results->preferred_acm->sector_count);
        pa_results->preferred_acm->sector_count = from.preferred_acm->sector_count;
    }
    pa_results->saturation_factor = from.saturation_factor;
}

void
RuntimeParamsHelper::copySaResults(IPU3AICRuntimeParams &to, ia_aiq_sa_results &from)
{
    ia_aiq_sa_results *sa_results;
    sa_results = (ia_aiq_sa_results*) to.sa_results;
    int size = from.width * from.height * sizeof(unsigned short);
    int to_size = sa_results->width * sa_results->height * sizeof(unsigned short);
    for (int i = 0; i < MAX_BAYER_ORDER_NUM; i++) {
        for (int j = 0; j < MAX_BAYER_ORDER_NUM; j++) {
            if (sa_results->lsc_grid[i][j] && from.lsc_grid[i][j]) {
                MEMCPY_S(sa_results->lsc_grid[i][j], to_size, from.lsc_grid[i][j], size);
            }
        }
    }
    sa_results->fraction_bits = from.fraction_bits;
    sa_results->color_order = from.color_order;
    sa_results->frame_params = from.frame_params;
    sa_results->height = from.height;
    memcpy(sa_results->light_source, from.light_source,
           sizeof(sa_results->light_source));
    sa_results->lsc_update = from.lsc_update;
    sa_results->width = from.width;
}

void
RuntimeParamsHelper::copyWeightGrid(IPU3AICRuntimeParams &to, ia_aiq_hist_weight_grid *from)
{
    ia_aiq_hist_weight_grid *weight_grid;
    weight_grid = (ia_aiq_hist_weight_grid*) to.weight_grid;
    weight_grid->height = from->height;
    int size = weight_grid->height * from->height * sizeof(*weight_grid->weights);
    memcpy(weight_grid->weights, from->weights, size);
    weight_grid->width = from->width;
}

#ifdef REMOTE_3A_SERVER
void
RuntimeParamsHelper::copyPaResultsMod(IPU3AICRuntimeParams &to, ia_aiq_pa_results_data &from)
{
    ia_aiq_pa_results *pa_results;
    pa_results = (ia_aiq_pa_results*) to.pa_results;
    pa_results->black_level = from.black_level;
    pa_results->brightness_level = from.brightness_level;
    memcpy(pa_results->color_conversion_matrix, from.color_conversion_matrix,
           sizeof(pa_results->color_conversion_matrix));
    pa_results->color_gains = from.color_gains;

    if (pa_results->ir_weight != nullptr) {
        pa_results->ir_weight->height = from.ir_weight.height;
        pa_results->ir_weight->width = from.ir_weight.width;
        int size = pa_results->ir_weight->height * pa_results->ir_weight->width;
        MEMCPY_S(pa_results->ir_weight->ir_weight_grid_B, sizeof(*pa_results->ir_weight->ir_weight_grid_B)*size,
             from.ir_weight.ir_weight_grid_B, sizeof(*from.ir_weight.ir_weight_grid_B)*size);
        MEMCPY_S(pa_results->ir_weight->ir_weight_grid_G, sizeof(*pa_results->ir_weight->ir_weight_grid_G)*size,
            from.ir_weight.ir_weight_grid_G, sizeof(*from.ir_weight.ir_weight_grid_G)*size);
        MEMCPY_S(pa_results->ir_weight->ir_weight_grid_R, sizeof(*pa_results->ir_weight->ir_weight_grid_R)*size,
            from.ir_weight.ir_weight_grid_R, sizeof(*from.ir_weight.ir_weight_grid_R)*size);
    }

    memcpy(pa_results->linearization.b, from.linearization.b,
            sizeof(*pa_results->linearization.b) * pa_results->linearization.size);
    memcpy(pa_results->linearization.gb, from.linearization.gb,
            sizeof(*pa_results->linearization.gb) * pa_results->linearization.size);
    memcpy(pa_results->linearization.gr, from.linearization.gr,
            sizeof(*pa_results->linearization.gr) * pa_results->linearization.size);
    memcpy(pa_results->linearization.r, from.linearization.r,
            sizeof(*pa_results->linearization.r) * pa_results->linearization.size);

    if (pa_results->preferred_acm != nullptr) {
        pa_results->preferred_acm->sector_count = from.preferred_acm.sector_count;
        MEMCPY_S(pa_results->preferred_acm->advanced_color_conversion_matrices,
            sizeof(*pa_results->preferred_acm->advanced_color_conversion_matrices) * pa_results->preferred_acm->sector_count,
            from.preferred_acm.advanced_color_conversion_matrices,
            sizeof(*from.preferred_acm.advanced_color_conversion_matrices) * from.preferred_acm.sector_count);
        MEMCPY_S(pa_results->preferred_acm->hue_of_sectors,
            sizeof(*pa_results->preferred_acm->hue_of_sectors) * pa_results->preferred_acm->sector_count,
            from.preferred_acm.hue_of_sectors,
            sizeof(*from.preferred_acm.hue_of_sectors) * pa_results->preferred_acm->sector_count);
    }
    pa_results->saturation_factor = from.saturation_factor;
}

void
RuntimeParamsHelper::copySaResultsMod(IPU3AICRuntimeParams &to, ia_aiq_sa_results_data &from)
{
    ia_aiq_sa_results *sa_results;
    sa_results = (ia_aiq_sa_results*) to.sa_results;
    int size = from.width * from.height;
    memcpy(sa_results->channel_b, from.channel_b,
            sizeof(*sa_results->channel_b) * size);
    memcpy(sa_results->channel_gb, from.channel_gb,
            sizeof(*sa_results->channel_gb) * size);
    memcpy(sa_results->channel_gr, from.channel_gr,
            sizeof(*sa_results->channel_gr) * size);
    memcpy(sa_results->channel_r, from.channel_r,
            sizeof(*sa_results->channel_r) * size);
    sa_results->covered_area = from.covered_area;
    sa_results->frame_params = from.frame_params;
    sa_results->height = from.height;
    memcpy(sa_results->light_source, from.light_source,
           sizeof(sa_results->light_source));
    sa_results->lsc_update = from.lsc_update;
    sa_results->num_patches = from.num_patches;
    sa_results->scene_difficulty = from.scene_difficulty;
    sa_results->width = from.width;
}

void
RuntimeParamsHelper::copyWeightGridMod(IPU3AICRuntimeParams &to, ia_aiq_hist_weight_grid_data &from)
{
    ia_aiq_hist_weight_grid *weight_grid;
    weight_grid = (ia_aiq_hist_weight_grid*) to.weight_grid;
    int size = from.height * from.width * sizeof(*weight_grid->weights);
    weight_grid->height = from.height;
    memcpy(weight_grid->weights, from.weights, size);
    weight_grid->width = from.width;
}
#endif


status_t
RuntimeParamsHelper::allocateAiStructs(IPU3AICRuntimeParams &runtimeParams)
{
    CLEAR(runtimeParams);
    ia_aiq_output_frame_parameters_t *frame = new ia_aiq_output_frame_parameters_t;
    memset(frame, 0, sizeof(ia_aiq_output_frame_parameters_t));
    runtimeParams.output_frame_params = frame;

    aic_resolution_config_parameters_t *params = new aic_resolution_config_parameters_t;
    memset(params, 0, sizeof(aic_resolution_config_parameters_t));
    runtimeParams.frame_resolution_parameters = params;

    aic_input_frame_parameters_t *frameParams = new aic_input_frame_parameters_t;
    memset(frameParams, 0, sizeof(aic_input_frame_parameters_t));
    runtimeParams.input_frame_params = frameParams;

    ia_aiq_gbce_results *gbce_results = new ia_aiq_gbce_results;
    memset(gbce_results, 0, sizeof(ia_aiq_gbce_results));
    runtimeParams.gbce_results = gbce_results;

    ia_aiq_awb_results *pAwb_results = new ia_aiq_awb_results;
    memset(pAwb_results, 0, sizeof(ia_aiq_awb_results));
    runtimeParams.awb_results = pAwb_results;

    ia_aiq_exposure_parameters *exposure_results = new ia_aiq_exposure_parameters;
    memset(exposure_results, 0, sizeof(ia_aiq_exposure_parameters));
    runtimeParams.exposure_results = exposure_results;

    ia_rectangle *focus_rect = new ia_rectangle;
    memset(focus_rect, 0, sizeof(ia_rectangle));
    runtimeParams.focus_rect = focus_rect;

    ia_aiq_pa_results *pa_results = new ia_aiq_pa_results;
    memset(pa_results, 0, sizeof(ia_aiq_pa_results));
    runtimeParams.pa_results = pa_results;

    ia_aiq_advanced_ccm_t *preferred_acm = new ia_aiq_advanced_ccm_t;
    memset(preferred_acm, 0, sizeof(ia_aiq_advanced_ccm_t));
    pa_results->preferred_acm = preferred_acm;

    ia_aiq_hist_weight_grid *weight_grid = new ia_aiq_hist_weight_grid;
    memset(weight_grid, 0, sizeof(ia_aiq_hist_weight_grid));
    runtimeParams.weight_grid = weight_grid;

    unsigned char *weights = new unsigned char[128 * 128];
    memset(weights, 0, 128 * 128);
    weight_grid->weights = weights;


    return OK;
}

void
RuntimeParamsHelper::deleteAiStructs(IPU3AICRuntimeParams &runtimeParams)
{
    delete runtimeParams.output_frame_params;
    runtimeParams.output_frame_params = nullptr;

    delete runtimeParams.frame_resolution_parameters;
    runtimeParams.frame_resolution_parameters = nullptr;

    delete runtimeParams.input_frame_params;
    runtimeParams.input_frame_params = nullptr;

    delete runtimeParams.gbce_results;
    runtimeParams.gbce_results = nullptr;

    delete runtimeParams.awb_results;
    runtimeParams.awb_results = nullptr;

    delete runtimeParams.exposure_results;
    runtimeParams.exposure_results = nullptr;

    delete runtimeParams.focus_rect;
    runtimeParams.focus_rect = nullptr;

    delete runtimeParams.pa_results->ir_weight;

    delete runtimeParams.pa_results->preferred_acm;

    delete runtimeParams.pa_results;
    runtimeParams.pa_results = nullptr;

    if (runtimeParams.sa_results !=nullptr){
        if (runtimeParams.sa_results->lsc_grid[0][0] != nullptr)
            delete runtimeParams.sa_results->lsc_grid[0][0];
        if (runtimeParams.sa_results->lsc_grid[0][1] != nullptr)
            delete runtimeParams.sa_results->lsc_grid[0][1];
        if (runtimeParams.sa_results->lsc_grid[1][0] != nullptr)
            delete runtimeParams.sa_results->lsc_grid[1][0];
        if (runtimeParams.sa_results->lsc_grid[1][1] != nullptr)
            delete runtimeParams.sa_results->lsc_grid[1][1];
    }

    delete runtimeParams.sa_results;
    runtimeParams.pa_results = nullptr;

    delete [] runtimeParams.weight_grid->weights;

    delete runtimeParams.weight_grid;
    runtimeParams.weight_grid = nullptr;

}

} /* namespace camera2 */
} /* namespace android */
