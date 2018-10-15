/*
 * Copyright (C) 2016-2017 Intel Corporation
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

#define LOG_TAG "IPC_CMC"

#include <utils/Errors.h>

#include "common/UtilityMacros.h"
#include "LogHelper.h"
#include "IPCCmc.h"

NAMESPACE_DECLARATION {
IPCCmc::IPCCmc()
{
    LOG1("@%s", __FUNCTION__);
}

IPCCmc::~IPCCmc()
{
    LOG1("@%s", __FUNCTION__);
}

bool IPCCmc::clientFlattenInit(const ia_binary_data& aiqb, cmc_init_params* params)
{
    LOG1("@%s, aiqb: data:%p, size:%d, params:%p", __FUNCTION__, aiqb.data, aiqb.size, params);
    CheckError(params == nullptr, false, "@%s, pData is nullptr", __FUNCTION__);
    CheckError(aiqb.data == nullptr, false, "@%s, aiqb_data.data is nullptr", __FUNCTION__);
    CheckError(aiqb.size == 0, false, "@%s, aiqb_data.size is 0", __FUNCTION__);
    CheckError(aiqb.size > sizeof(params->input.data),
        false, "@%s, aiqb:%d is too big", __FUNCTION__, aiqb.size);

    ia_binary_data_mod* input = &params->input;
    MEMCPY_S(input->data, sizeof(input->data), aiqb.data, aiqb.size);
    input->size = aiqb.size;

    return true;
}

bool IPCCmc::clientUnflattenInit(const cmc_init_params& params, ia_cmc_t** cmc, uintptr_t* cmcRemoteHandle)
{
    LOG1("@%s, cmc:%p", __FUNCTION__, cmc);
    CheckError(cmc == nullptr, false, "@%s, cmc is nullptr", __FUNCTION__);

    ia_cmc_data* results = const_cast<ia_cmc_data*>(&params.results);
    ia_cmc_t* base = &results->base;

    if (base->cmc_general_data) {
        base->cmc_general_data = &results->cmc_general_data;
    }

    if (base->cmc_parsed_black_level.cmc_black_level) {
        base->cmc_parsed_black_level.cmc_black_level = &results->cmc_parsed_black_level.cmc_black_level;
    }
    if (base->cmc_parsed_black_level.cmc_black_level_luts) {
        base->cmc_parsed_black_level.cmc_black_level_luts = &results->cmc_parsed_black_level.cmc_black_level_luts;
    }

    if (base->cmc_saturation_level) {
        base->cmc_saturation_level = &results->cmc_saturation_level;
    }

    if (base->cmc_sensitivity) {
        base->cmc_sensitivity = &results->cmc_sensitivity;
    }

    if (base->cmc_parsed_lens_shading.cmc_lens_shading) {
        base->cmc_parsed_lens_shading.cmc_lens_shading = &results->cmc_parsed_lens_shading.cmc_lens_shading;
    }
    if (base->cmc_parsed_lens_shading.cmc_lsc_grids) {
        base->cmc_parsed_lens_shading.cmc_lsc_grids = &results->cmc_parsed_lens_shading.cmc_lsc_grids;
    }
    if (base->cmc_parsed_lens_shading.lsc_grids) {
        base->cmc_parsed_lens_shading.lsc_grids = &results->cmc_parsed_lens_shading.lsc_grids;
    }
    if (base->cmc_parsed_lens_shading.cmc_lsc_rg_bg_ratios) {
        base->cmc_parsed_lens_shading.cmc_lsc_rg_bg_ratios = &results->cmc_parsed_lens_shading.cmc_lsc_rg_bg_ratios;
    }

    if (base->cmc_parsed_optics.cmc_optomechanics) {
        base->cmc_parsed_optics.cmc_optomechanics = &results->cmc_parsed_optics.cmc_optomechanics;
    }
    if (base->cmc_parsed_optics.lut_apertures) {
        base->cmc_parsed_optics.lut_apertures = &results->cmc_parsed_optics.lut_apertures;
    }

    if (base->cmc_parsed_color_matrices.cmc_color_matrices) {
        base->cmc_parsed_color_matrices.cmc_color_matrices = &results->cmc_parsed_color_matrices.cmc_color_matrices;
    }
    if (base->cmc_parsed_color_matrices.cmc_color_matrix) {
        base->cmc_parsed_color_matrices.cmc_color_matrix = &results->cmc_parsed_color_matrices.cmc_color_matrix;
    }
    if (base->cmc_parsed_color_matrices.ccm_estimate_method) {
        base->cmc_parsed_color_matrices.ccm_estimate_method = &results->cmc_parsed_color_matrices.ccm_estimate_method;
    }

    if (base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion) {
        base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion =
            &results->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion;

        if (base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_segments) {
            base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_segments =
                results->cmc_parsed_analog_gain_conversion.cmc_analog_gain_segments;
        }
        if (base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_pairs) {
            base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_pairs =
                results->cmc_parsed_analog_gain_conversion.cmc_analog_gain_pairs;
        }
    }

    *cmc = &results->base;
    *cmcRemoteHandle = results->cmcRemoteHandle;

    return true;
}

bool IPCCmc::serverUnflattenInit(const cmc_init_params& params, ia_binary_data* aiqb)
{
    LOG1("@%s, aiqb:%p", __FUNCTION__, aiqb);
    CheckError(aiqb == nullptr, false, "@%s, aiqbData is nullptr", __FUNCTION__);

    ia_binary_data_mod* input = const_cast<ia_binary_data_mod*>(&params.input);
    aiqb->data = input->data;
    aiqb->size = input->size;

    return true;
}

bool IPCCmc::serverFlattenInit(const ia_cmc_t& cmc, cmc_init_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    ia_cmc_data* results = &params->results;
    ia_cmc_t* base = &results->base;

    *base = cmc;
    results->cmcRemoteHandle = reinterpret_cast<uintptr_t>(&cmc);

    if (base->cmc_general_data) {
        results->cmc_general_data = *base->cmc_general_data;
    }

    if (base->cmc_parsed_black_level.cmc_black_level) {
        results->cmc_parsed_black_level.cmc_black_level = *base->cmc_parsed_black_level.cmc_black_level;
    }
    if (base->cmc_parsed_black_level.cmc_black_level_luts) {
        results->cmc_parsed_black_level.cmc_black_level_luts = *base->cmc_parsed_black_level.cmc_black_level_luts;
    }

    if (base->cmc_saturation_level) {
        results->cmc_saturation_level = *base->cmc_saturation_level;
    }

    if (base->cmc_sensitivity) {
        results->cmc_sensitivity = *base->cmc_sensitivity;
    }

    if (base->cmc_parsed_lens_shading.cmc_lens_shading) {
        results->cmc_parsed_lens_shading.cmc_lens_shading = *base->cmc_parsed_lens_shading.cmc_lens_shading;
    }
    if (base->cmc_parsed_lens_shading.cmc_lsc_grids) {
        results->cmc_parsed_lens_shading.cmc_lsc_grids = *base->cmc_parsed_lens_shading.cmc_lsc_grids;
    }
    if (base->cmc_parsed_lens_shading.lsc_grids) {
        results->cmc_parsed_lens_shading.lsc_grids = *base->cmc_parsed_lens_shading.lsc_grids;
    }
    if (base->cmc_parsed_lens_shading.cmc_lsc_rg_bg_ratios) {
        results->cmc_parsed_lens_shading.cmc_lsc_rg_bg_ratios = *base->cmc_parsed_lens_shading.cmc_lsc_rg_bg_ratios;
    }

    if (base->cmc_parsed_optics.cmc_optomechanics) {
        results->cmc_parsed_optics.cmc_optomechanics = *base->cmc_parsed_optics.cmc_optomechanics;
    }
    if (base->cmc_parsed_optics.lut_apertures) {
        results->cmc_parsed_optics.lut_apertures = *base->cmc_parsed_optics.lut_apertures;
    }

    if (base->cmc_parsed_color_matrices.cmc_color_matrices) {
        results->cmc_parsed_color_matrices.cmc_color_matrices = *base->cmc_parsed_color_matrices.cmc_color_matrices;
    }
    if (base->cmc_parsed_color_matrices.cmc_color_matrix) {
        //fix asan issue:base->cmc_parsed_color_matrices.cmc_color_matrix is not 4 aligned
        //use memcpy instead of *
        memcpy(&results->cmc_parsed_color_matrices.cmc_color_matrix,
               base->cmc_parsed_color_matrices.cmc_color_matrix,
               sizeof(results->cmc_parsed_color_matrices.cmc_color_matrix));
    }
    if (base->cmc_parsed_color_matrices.ccm_estimate_method) {
        results->cmc_parsed_color_matrices.ccm_estimate_method = *base->cmc_parsed_color_matrices.ccm_estimate_method;
    }

    if (base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion) {
        results->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion =
            *base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion;

        CheckError(base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion->num_segments > MAX_NUM_SEGMENTS,
            false, "@%s, num_segments:%d is too big", __FUNCTION__,
            base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion->num_segments);
        CheckError(base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion->num_pairs > MAX_NUM_ANALOG_PAIRS, false,
            "@%s, num_pairs:%d is too big", __FUNCTION__,
            base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion->num_pairs);

        if (base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_segments) {
            MEMCPY_S(results->cmc_parsed_analog_gain_conversion.cmc_analog_gain_segments,
                sizeof(results->cmc_parsed_analog_gain_conversion.cmc_analog_gain_segments),
                base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_segments,
                sizeof(*base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_segments)
                * base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion->num_segments);
        }
        if (base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_pairs) {
            MEMCPY_S(results->cmc_parsed_analog_gain_conversion.cmc_analog_gain_pairs,
                sizeof(results->cmc_parsed_analog_gain_conversion.cmc_analog_gain_pairs),
                base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_pairs,
                sizeof(*base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_pairs)
                * base->cmc_parsed_analog_gain_conversion.cmc_analog_gain_conversion->num_pairs);
        }
    }

    return true;
}
} NAMESPACE_DECLARATION_END
