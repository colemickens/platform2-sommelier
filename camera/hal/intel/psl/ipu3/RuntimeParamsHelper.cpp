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

#define LOG_TAG "AicLibrary"

#include <string.h>
#include "RuntimeParamsHelper.h"
#include "IPU3ISPPipe.h"
#include "LogHelper.h"
#include <ia_aiq_types.h>

#define AIC_GRID_SIZE (128*128)

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
    MEMCPY_S(pa_results->color_conversion_matrix, sizeof(pa_results->color_conversion_matrix),
        from.color_conversion_matrix, sizeof(from.color_conversion_matrix));
    pa_results->color_gains = from.color_gains;
    if (from.ir_weight && pa_results->ir_weight) {
        int size = pa_results->ir_weight->height * pa_results->ir_weight->width;
        MEMCPY_S(pa_results->ir_weight->ir_weight_grid_B,
            sizeof(*pa_results->ir_weight->ir_weight_grid_B)*size,
            from.ir_weight->ir_weight_grid_B,
            sizeof(*from.ir_weight->ir_weight_grid_B)*size);
        MEMCPY_S(pa_results->ir_weight->ir_weight_grid_G,
            sizeof(*pa_results->ir_weight->ir_weight_grid_G)*size,
            from.ir_weight->ir_weight_grid_G,
            sizeof(*from.ir_weight->ir_weight_grid_G)*size);
        MEMCPY_S(pa_results->ir_weight->ir_weight_grid_R,
            sizeof(pa_results->ir_weight->ir_weight_grid_R)*size,
            from.ir_weight->ir_weight_grid_R,
            sizeof(*from.ir_weight->ir_weight_grid_R)*size);
    }
    int size = pa_results->linearization.size;
    MEMCPY_S(pa_results->linearization.b, sizeof(*pa_results->linearization.b)*size,
        from.linearization.b, sizeof(*from.linearization.b) * size);
    MEMCPY_S(pa_results->linearization.gb, sizeof(*pa_results->linearization.gb) * size,
        from.linearization.gb, sizeof(*from.linearization.gb) * size);
    MEMCPY_S(pa_results->linearization.gr, sizeof(*pa_results->linearization.gr) * size,
        from.linearization.gr, sizeof(*from.linearization.gr) * size);
    MEMCPY_S(pa_results->linearization.r, sizeof(*pa_results->linearization.r) * size,
        from.linearization.r, sizeof(*from.linearization.r) * size);
    if (from.preferred_acm && pa_results->preferred_acm) {
        pa_results->preferred_acm->sector_count = from.preferred_acm->sector_count;
        MEMCPY_S(pa_results->preferred_acm->advanced_color_conversion_matrices,
            sizeof(*pa_results->preferred_acm->advanced_color_conversion_matrices) * pa_results->preferred_acm->sector_count,
            from.preferred_acm->advanced_color_conversion_matrices,
            sizeof(*pa_results->preferred_acm->advanced_color_conversion_matrices) * from.preferred_acm->sector_count);
        MEMCPY_S(pa_results->preferred_acm->hue_of_sectors,
            sizeof(*pa_results->preferred_acm->hue_of_sectors) * pa_results->preferred_acm->sector_count,
            from.preferred_acm->hue_of_sectors,
            sizeof(*from.preferred_acm->hue_of_sectors) * from.preferred_acm->sector_count);
    }
    pa_results->saturation_factor = from.saturation_factor;
}

void
RuntimeParamsHelper::copySaResults(IPU3AICRuntimeParams &to, ia_aiq_sa_results &from)
{
    ia_aiq_sa_results *sa_results;
    sa_results = (ia_aiq_sa_results*) to.sa_results;
    int size = from.width * from.height;
    MEMCPY_S(sa_results->channel_b, sizeof(*sa_results->channel_b) * size,
        from.channel_b, sizeof(*from.channel_b) * size);
    MEMCPY_S(sa_results->channel_gb, sizeof(*sa_results->channel_gb) * size,
        from.channel_gb, sizeof(*from.channel_gb) * size);
    MEMCPY_S(sa_results->channel_gr, sizeof(*sa_results->channel_gr) * size,
        from.channel_gr, sizeof(*from.channel_gr) * size);
    MEMCPY_S(sa_results->channel_r, sizeof(*sa_results->channel_r) * size,
        from.channel_r, sizeof(*from.channel_r) * size);
    sa_results->covered_area = from.covered_area;
    sa_results->frame_params = from.frame_params;
    sa_results->height = from.height;
    MEMCPY_S(sa_results->light_source, sizeof(sa_results->light_source),
        from.light_source, sizeof(from.light_source));
    sa_results->lsc_update = from.lsc_update;
    sa_results->num_patches = from.num_patches;
    sa_results->scene_difficulty = from.scene_difficulty;
    sa_results->width = from.width;
}

void
RuntimeParamsHelper::copyWeightGrid(IPU3AICRuntimeParams &to, ia_aiq_hist_weight_grid *from)
{
    ia_aiq_hist_weight_grid *weight_grid;
    weight_grid = (ia_aiq_hist_weight_grid*) to.weight_grid;
    weight_grid->height = from->height;
    weight_grid->width = from->width;
    size_t size = weight_grid->height * weight_grid->width * sizeof(*weight_grid->weights);
    MEMCPY_S(weight_grid->weights, size, from->weights, size);
}

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

    ia_aiq_sa_results *sa_results = new ia_aiq_sa_results;
    memset(sa_results, 0, sizeof(ia_aiq_sa_results));
    runtimeParams.sa_results = sa_results;

    float *channel_b = new float[AIC_GRID_SIZE];
    memset(channel_b, 0, AIC_GRID_SIZE);
    sa_results->channel_b = channel_b;

    float *channel_gb = new float[AIC_GRID_SIZE];
    memset(channel_gb, 0, AIC_GRID_SIZE);
    sa_results->channel_gb = channel_gb;

    float *channel_gr = new float[AIC_GRID_SIZE];
    memset(channel_gr, 0, AIC_GRID_SIZE);
    sa_results->channel_gr = channel_gr;

    float *channel_r = new float[AIC_GRID_SIZE];
    memset(channel_r, 0, AIC_GRID_SIZE);
    sa_results->channel_r = channel_r;

    ia_aiq_pa_results *pa_results = new ia_aiq_pa_results;
    memset(pa_results, 0, sizeof(ia_aiq_pa_results));
    runtimeParams.pa_results = pa_results;

    ia_aiq_advanced_ccm_t *preferred_acm = new ia_aiq_advanced_ccm_t;
    memset(preferred_acm, 0, sizeof(ia_aiq_advanced_ccm_t));
    pa_results->preferred_acm = preferred_acm;

    ia_aiq_hist_weight_grid *weight_grid = new ia_aiq_hist_weight_grid;
    memset(weight_grid, 0, sizeof(ia_aiq_hist_weight_grid));
    runtimeParams.weight_grid = weight_grid;

    unsigned char *weights = new unsigned char[AIC_GRID_SIZE];
    memset(weights, 0, AIC_GRID_SIZE);
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

    delete [] runtimeParams.sa_results->channel_b;
    delete [] runtimeParams.sa_results->channel_gb;
    delete [] runtimeParams.sa_results->channel_gr;
    delete [] runtimeParams.sa_results->channel_r;

    delete runtimeParams.sa_results;
    runtimeParams.pa_results = nullptr;

    delete [] runtimeParams.weight_grid->weights;

    delete runtimeParams.weight_grid;
    runtimeParams.weight_grid = nullptr;

}

} /* namespace camera2 */
} /* namespace android */
