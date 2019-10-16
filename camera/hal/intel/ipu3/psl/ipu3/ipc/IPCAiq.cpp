/*
 * Copyright (C) 2016-2018 Intel Corporation
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

#define LOG_TAG "IPC_AIQ"

#include <ia_types.h>
#include <utils/Errors.h>

#include "common/UtilityMacros.h"
#include "LogHelper.h"
#include "IPCAiq.h"

namespace cros {
namespace intel {
IPCAiq::IPCAiq()
{
    LOG1("@%s", __FUNCTION__);
}

IPCAiq::~IPCAiq()
{
    LOG1("@%s", __FUNCTION__);
}

// init
bool IPCAiq::clientFlattenInit(const ia_binary_data* aiqb_data, unsigned int aiqbSize,
            const ia_binary_data* nvm_data, unsigned int nvmSize,
            const ia_binary_data* aiqd_data,unsigned int aiqdSize,
            unsigned int statsMaxWidth,
            unsigned int statsMaxHeight,
            unsigned int maxNumStatsIn,
            uintptr_t mkn,
            uintptr_t cmc,
            uint8_t* pData, unsigned int size)
{
    LOG1("@%s, aiqb_data:%p, nvm_data:%p, aiqd_data:%p", __FUNCTION__, aiqb_data, nvm_data, aiqd_data);
    CheckError(pData == nullptr, false, "@%s, pData is nullptr", __FUNCTION__);

    if (aiqb_data)
        LOG2("aiqb_data->size:%d", aiqb_data->size);
    if (nvm_data)
        LOG2("nvm_data->size:%d", nvm_data->size);
    if (aiqd_data)
        LOG2("aiqd_data->size:%d", aiqd_data->size);

    uint8_t* ptr = pData;
    memset(ptr, 0, size);

    aiq_init_params* params = (aiq_init_params*)ptr;
    params->aiqb_size = aiqbSize;
    params->nvm_size = nvmSize;
    params->aiqd_size = aiqdSize;
    params->stats_max_width = statsMaxWidth;
    params->stats_max_height = statsMaxHeight;
    params->max_num_stats_in = maxNumStatsIn;
    params->ia_mkn = mkn;
    params->cmcRemoteHandle = cmc;

    ptr += sizeof(aiq_init_params);
    if (aiqbSize && aiqb_data) {
        MEMCPY_S(ptr, aiqbSize, aiqb_data->data, aiqbSize);
    }

    ptr += aiqbSize;
    if (nvmSize && nvm_data) {
        MEMCPY_S(ptr, nvmSize, nvm_data->data, nvmSize);
    }

    ptr += nvmSize;
    if (aiqdSize && aiqd_data) {
        MEMCPY_S(ptr, aiqdSize, aiqd_data->data, aiqdSize);
    }

    return true;
}

bool IPCAiq::serverUnflattenInit(void* pData, int dataSize, ia_binary_data* aiqbData, ia_binary_data* nvmData, ia_binary_data* aiqdData)
{
    LOG1("@%s, pData:%p, dataSize:%d, aiqbData:%p, nvmData:%p, aiqdData:%p",
        __FUNCTION__, pData, dataSize, aiqbData, nvmData, aiqdData);
    CheckError((dataSize < sizeof(aiq_init_params)), false, "@%s, buffer is small", __FUNCTION__);
    CheckError((pData == nullptr), false, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((aiqbData == nullptr), false, "@%s, aiqbData is nullptr", __FUNCTION__);
    CheckError((nvmData == nullptr), false, "@%s, nvmData is nullptr", __FUNCTION__);
    CheckError((aiqdData == nullptr), false, "@%s, aiqdData is nullptr", __FUNCTION__);

    aiq_init_params* params = static_cast<aiq_init_params*>(pData);

    LOG2("@%s, aiqb_size:%d, nvm_size:%d, aiqd_size:%d",
        __FUNCTION__, params->aiqb_size, params->nvm_size, params->aiqd_size);

    if (dataSize < (sizeof(aiq_init_params) + params->aiqb_size + params->nvm_size + params->aiqd_size)) {
        LOGE("@%s, dataSize:%d is too small", __FUNCTION__, dataSize);
        return false;
    }

    uint8_t* ptr = (uint8_t*)((aiq_init_params*)pData + 1);
    aiqbData->size = params->aiqb_size;
    aiqbData->data = ptr;

    ptr += params->aiqb_size;
    nvmData->size = params->nvm_size;
    nvmData->data = ptr;

    ptr += params->nvm_size;
    aiqdData->size = params->aiqd_size;
    aiqdData->data = ptr;

    return true;
}

// af
bool IPCAiq::clientFlattenAf(uintptr_t aiq, const ia_aiq_af_input_params& inParams, af_run_params* params)
{
    LOG1("@%s, aiq:0x%" PRIxPTR ", params:%p", __FUNCTION__, aiq, params);
    CheckError(reinterpret_cast<ia_aiq*>(aiq) == nullptr, false, "@%s, aiq is nullptr", __FUNCTION__);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    params->aiq_handle = aiq;

    params->base = inParams;
    ia_aiq_af_input_params* base = &params->base;
    if (base->focus_rect) {
        params->focus_rect = *inParams.focus_rect;
    }
    if (base->manual_focus_parameters) {
        params->manual_focus_parameters = *inParams.manual_focus_parameters;
    }

    return true;
}

bool IPCAiq::clientUnflattenAf(const af_run_params& params, ia_aiq_af_results** results)
{
    LOG1("@%s, results:%p", __FUNCTION__, results);

    CheckError((results == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    *results = const_cast<ia_aiq_af_results*>(&params.results);

    return true;
}

bool IPCAiq::serverUnflattenAf(const af_run_params& inParams, ia_aiq_af_input_params** params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    ia_aiq_af_input_params* base = const_cast<ia_aiq_af_input_params*>(&inParams.base);
    if (base->focus_rect) {
        base->focus_rect = const_cast<ia_rectangle*>(&inParams.focus_rect);
    }
    if (base->manual_focus_parameters) {
        base->manual_focus_parameters =
            const_cast<ia_aiq_manual_focus_parameters*>(&inParams.manual_focus_parameters);
    }

    *params = base;

    return true;
}

bool IPCAiq::serverFlattenAf(const ia_aiq_af_results& afResults, af_run_params* params)
{
    LOG1("@%s, results:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    ia_aiq_af_results* results = &params->results;
    *results = afResults;

    LOG2("af results->status:%d", results->status);
    LOG2("af results->current_focus_distance:%d", results->current_focus_distance);
    LOG2("af results->next_lens_position:%d", results->next_lens_position);
    LOG2("af results->lens_driver_action:%d", results->lens_driver_action);
    LOG2("af results->use_af_assist:%d", results->use_af_assist);
    LOG2("af results->final_lens_position_reached:%d", results->final_lens_position_reached);

    return true;
}

// gbce
bool IPCAiq::clientFlattenGbce(uintptr_t aiq, const ia_aiq_gbce_input_params& inParams, gbce_run_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__,  params);
    CheckError(reinterpret_cast<ia_aiq*>(aiq) == nullptr, false, "@%s, aiq is nullptr", __FUNCTION__);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    params->aiq_handle = aiq;
    params->base = inParams;

    return true;
}

bool IPCAiq::clientUnflattenGbce(const gbce_run_params& params, ia_aiq_gbce_results** results)
{
    LOG1("@%s, results:%p", __FUNCTION__, results);
    CheckError((results == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    ia_aiq_gbce_results* resBase = const_cast<ia_aiq_gbce_results*>(&params.resBase);

    if (resBase->r_gamma_lut) {
        resBase->r_gamma_lut = const_cast<float*>(params.r_gamma_lut);
    }

    if (resBase->b_gamma_lut) {
        resBase->b_gamma_lut = const_cast<float*>(params.b_gamma_lut);
    }

    if (resBase->g_gamma_lut) {
        resBase->g_gamma_lut = const_cast<float*>(params.g_gamma_lut);
    }

    if (resBase->tone_map_lut) {
        resBase->tone_map_lut = const_cast<float*>(params.tone_map_lut);
    }

    CheckError((resBase->gamma_lut_size > MAX_NUM_GAMMA_LUTS), false,
        "@%s, resBase->gamma_lut_size:%d is too big", __FUNCTION__, resBase->gamma_lut_size);
    CheckError((resBase->tone_map_lut_size > MAX_NUM_TOME_MAP_LUTS), false,
        "@%s, resBase->tone_map_lut_size:%d is too big", __FUNCTION__, resBase->tone_map_lut_size);
    LOG2("@%s, MAX_NUM_GAMMA_LUTS:%d, resBase->gamma_lut_size:%d",
        __FUNCTION__, MAX_NUM_GAMMA_LUTS, resBase->gamma_lut_size);
    LOG2("@%s, MAX_NUM_TOME_MAP_LUTS:%d, resBase->tone_map_lut_size:%d",
        __FUNCTION__, MAX_NUM_TOME_MAP_LUTS, resBase->tone_map_lut_size);

    *results = resBase;

    return true;
}

bool IPCAiq::serverFlattenGbce(const ia_aiq_gbce_results& gbceResults, gbce_run_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    size_t size = gbceResults.gamma_lut_size * sizeof(*gbceResults.g_gamma_lut);
    LOG2("@%s, gamma_lut_size:%d, size:%zu, tone_map_lut_size:%d",
        __FUNCTION__, gbceResults.gamma_lut_size, size, gbceResults.tone_map_lut_size);

    params->resBase = gbceResults;
    const ia_aiq_gbce_results* resBase = &params->resBase;

    if (resBase->r_gamma_lut) {
        MEMCPY_S(params->r_gamma_lut, sizeof(params->r_gamma_lut), gbceResults.r_gamma_lut, size);
    }

    if (resBase->b_gamma_lut) {
        MEMCPY_S(params->b_gamma_lut, sizeof(params->b_gamma_lut), gbceResults.b_gamma_lut, size);
    }

    if (resBase->g_gamma_lut) {
        MEMCPY_S(params->g_gamma_lut, sizeof(params->g_gamma_lut), gbceResults.g_gamma_lut, size);
    }

    if (resBase->tone_map_lut) {
        MEMCPY_S(params->tone_map_lut, sizeof(params->tone_map_lut),
            gbceResults.tone_map_lut, gbceResults.tone_map_lut_size * sizeof(*gbceResults.tone_map_lut));
    }

    return true;
}

// statistics
bool IPCAiq::clientFlattenStat(uintptr_t aiq, const ia_aiq_statistics_input_params& inParams, set_statistics_params* params)
{
    LOG1("@%s, aiq:0x%" PRIxPTR ", params:%p", __FUNCTION__, aiq, params);
    CheckError(reinterpret_cast<ia_aiq*>(aiq) == nullptr, false, "@%s, aiq is nullptr", __FUNCTION__);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    params->ia_aiq = aiq;

    set_statistics_params_data* input = &params->input;
    input->base = inParams;
    ia_aiq_statistics_input_params* base = &input->base;

    if (base->frame_ae_parameters) {
        flattenAeResults(*base->frame_ae_parameters, &input->frame_ae_parameters);
    }

    if (base->frame_af_parameters) {
        input->frame_af_parameters = *base->frame_af_parameters;
    }

    if (base->rgbs_grids) {
        CheckError((base->num_rgbs_grids > MAX_NUMBER_OF_GRIDS), false,
            "@%s, num_rgbs_grids:%d > MAX_NUMBER_OF_GRIDS:%d",
            __FUNCTION__, base->num_rgbs_grids, MAX_NUMBER_OF_GRIDS);

        for (int i = 0; i < MAX_NUMBER_OF_GRIDS; i++) {
            ia_aiq_rgbs_grid_data* rgbs_grids = &input->rgbs_grids[i];
            rgbs_grids->base = *base->rgbs_grids[i];

            CheckError((rgbs_grids->base.grid_width * rgbs_grids->base.grid_height > MAX_NUM_BLOCKS), false,
                "@%s, grid_width:%d * grid_height:%d is too big",
                __FUNCTION__, rgbs_grids->base.grid_width, rgbs_grids->base.grid_height);

            MEMCPY_S(rgbs_grids->blocks_ptr,
                sizeof(rgbs_grids->blocks_ptr),
                rgbs_grids->base.blocks_ptr,
                rgbs_grids->base.grid_width * rgbs_grids->base.grid_height * sizeof(*rgbs_grids->base.blocks_ptr));
        }
    }

    if (base->hdr_rgbs_grid) {
        ia_aiq_hdr_rgbs_grid_data* hdr_rgbs_grid = &input->hdr_rgbs_grid;
        hdr_rgbs_grid->base = *base->hdr_rgbs_grid;
        MEMCPY_S(hdr_rgbs_grid->blocks_ptr,
            sizeof(hdr_rgbs_grid->blocks_ptr),
            hdr_rgbs_grid->base.blocks_ptr,
            hdr_rgbs_grid->base.grid_width * hdr_rgbs_grid->base.grid_height * sizeof(*hdr_rgbs_grid->base.blocks_ptr));
    }

    if (base->af_grids) {
        CheckError((base->num_af_grids > MAX_NUMBER_OF_AF_GRIDS), false,
            "@%s, num_af_grids:%d > MAX_NUMBER_OF_AF_GRIDS:%d",
            __FUNCTION__, base->num_af_grids, MAX_NUMBER_OF_AF_GRIDS);

        for (int i = 0; i < MAX_NUMBER_OF_AF_GRIDS; i++) {
            ia_aiq_af_grid_data* af_grids = &input->af_grids[i];
            af_grids->base = *base->af_grids[i];
            MEMCPY_S(af_grids->filter_response_1,
                sizeof(af_grids->filter_response_1),
                af_grids->base.filter_response_1,
                af_grids->base.grid_width * af_grids->base.grid_height * sizeof(*af_grids->base.filter_response_1));
            MEMCPY_S(af_grids->filter_response_2,
                sizeof(af_grids->filter_response_2),
                af_grids->base.filter_response_2,
                af_grids->base.grid_width * af_grids->base.grid_height * sizeof(*af_grids->base.filter_response_2));
        }
    }

    if (base->frame_pa_parameters) {
        flattenPaResults(*base->frame_pa_parameters, &input->frame_pa_parameters);
    }

    if (base->faces) {
        input->faces.base = *base->faces;
        for (int i = 0; i < input->faces.base.num_faces; i++) {
            input->faces.faces[i] = *(base->faces->faces + i);
        }
    }

    if (base->awb_results) {
        input->awb_results = *base->awb_results;
    }

    if (base->frame_sa_parameters) {
        flattenSaResults(*base->frame_sa_parameters, &input->frame_sa_parameters);
    }

    if (base->depth_grids) {
        CheckError((base->num_depth_grids > MAX_NUMBER_OF_DEPTH_GRIDS), false,
            "@%s, num_depth_grids:%d > MAX_NUMBER_OF_DEPTH_GRIDS:%d",
            __FUNCTION__, base->num_depth_grids, MAX_NUMBER_OF_DEPTH_GRIDS);

        for (int i = 0; i < MAX_NUMBER_OF_DEPTH_GRIDS; i++) {
            ia_aiq_depth_grid_data* depth_grids = &input->depth_grids[i];
            depth_grids->base = *base->depth_grids[i];
            MEMCPY_S(depth_grids->grid_rect,
                sizeof(depth_grids->grid_rect),
                depth_grids->base.grid_rect,
                depth_grids->base.grid_height * depth_grids->base.grid_width * sizeof(*depth_grids->base.grid_rect));
            MEMCPY_S(depth_grids->depth_data,
                sizeof(depth_grids->depth_data),
                depth_grids->base.depth_data,
                depth_grids->base.grid_height * depth_grids->base.grid_width * sizeof(*depth_grids->base.depth_data));
            MEMCPY_S(depth_grids->confidence,
                sizeof(depth_grids->confidence),
                depth_grids->base.confidence,
                depth_grids->base.grid_height * depth_grids->base.grid_width * sizeof(*depth_grids->base.confidence));
        }
    }

    return true;
}

bool IPCAiq::serverUnflattenStat(const set_statistics_params& inParams, ia_aiq_statistics_input_params** params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    set_statistics_params_data* input = const_cast<set_statistics_params_data*>(&inParams.input);
    ia_aiq_statistics_input_params* base = &input->base;

    if (base->frame_ae_parameters) {
        unflattenAeResults(&input->frame_ae_parameters);
        base->frame_ae_parameters = &input->frame_ae_parameters.base;
    }

    if (base->frame_af_parameters) {
        base->frame_af_parameters = &input->frame_af_parameters;
    }

    if (base->rgbs_grids) {
        CheckError((base->num_rgbs_grids > MAX_NUMBER_OF_GRIDS), false,
            "@%s, num_rgbs_grids:%d > MAX_NUMBER_OF_GRIDS:%d",
            __FUNCTION__, base->num_rgbs_grids, MAX_NUMBER_OF_GRIDS);

        for (int i = 0; i < base->num_rgbs_grids; i++) {
            ia_aiq_rgbs_grid_data* rgbs_grids = &input->rgbs_grids[i];
            rgbs_grids->base.blocks_ptr = rgbs_grids->blocks_ptr;

            input->rgbs_grids_array[i] = &rgbs_grids->base;
        }
        base->rgbs_grids = (input->rgbs_grids_array);
    }

    if (base->hdr_rgbs_grid) {
        input->hdr_rgbs_grid.base.blocks_ptr = input->hdr_rgbs_grid.blocks_ptr;
        base->hdr_rgbs_grid = &input->hdr_rgbs_grid.base;
    }

    if (base->af_grids) {
        CheckError((base->num_af_grids > MAX_NUMBER_OF_AF_GRIDS), false,
            "@%s, num_af_grids:%d > MAX_NUMBER_OF_AF_GRIDS:%d",
            __FUNCTION__, base->num_af_grids, MAX_NUMBER_OF_AF_GRIDS);

        for (int i = 0; i < base->num_af_grids; i++) {
            ia_aiq_af_grid_data* af_grids = &input->af_grids[i];
            af_grids->base.filter_response_1 = af_grids->filter_response_1;
            af_grids->base.filter_response_2 = af_grids->filter_response_2;

            input->af_grids_array[i] = &af_grids->base;
        }
        base->af_grids = input->af_grids_array;
    }

    if (base->frame_pa_parameters) {
        unflattenPaResults(&input->frame_pa_parameters);
        base->frame_pa_parameters = &input->frame_pa_parameters.base;
    }

    if (base->faces) {
        input->faces.base.faces = input->faces.faces;
        base->faces = &input->faces.base;
    }

    if (base->awb_results) {
        base->awb_results = &input->awb_results;
    }

    if (base->frame_sa_parameters) {
        unflattenSaResults(&input->frame_sa_parameters);
        base->frame_sa_parameters = &input->frame_sa_parameters.base;
    }

    if (base->depth_grids) {
        CheckError((base->num_depth_grids > MAX_NUMBER_OF_DEPTH_GRIDS), false,
            "@%s, num_depth_grids:%d > MAX_NUMBER_OF_DEPTH_GRIDS:%d",
            __FUNCTION__, base->num_depth_grids, MAX_NUMBER_OF_DEPTH_GRIDS);

        for (int i = 0; i < base->num_depth_grids; i++) {
            ia_aiq_depth_grid_data* depth_grids = &input->depth_grids[i];
            depth_grids->base.grid_rect = depth_grids->grid_rect;
            depth_grids->base.depth_data = depth_grids->depth_data;
            depth_grids->base.confidence = depth_grids->confidence;

            input->depth_grids_array[i] = &depth_grids->base;
        }
        base->depth_grids = input->depth_grids_array;
    }

    *params = base;

    return true;
}

// ae
bool IPCAiq::clientFlattenAe(uintptr_t aiq, const ia_aiq_ae_input_params& inParams, ae_run_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((reinterpret_cast<ia_aiq*>(aiq) == nullptr), false, "@%s, aiq is nullptr", __FUNCTION__);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    params->aiq_handle = aiq;

    params->base = inParams;
    const ia_aiq_ae_input_params* base = &params->base;

    if (base->aec_features) {
        params->aec_features = *inParams.aec_features;
    }

    if (base->exposure_coordinate) {
        params->exposure_coordinate = *inParams.exposure_coordinate;
    }

    if (base->exposure_window) {
        params->exposure_window = *inParams.exposure_window;
    }

    if (inParams.num_exposures > 1)
        LOGE("@%s, BUG: num_exposures:%d greater than one. Copying only first.",
            __FUNCTION__, inParams.num_exposures);

    if (base->sensor_descriptor) {
        params->sensor_descriptor = *inParams.sensor_descriptor;
    }

    if (base->manual_exposure_time_us) {
        params->manual_exposure_time_us = *inParams.manual_exposure_time_us;
    }

    if (base->manual_analog_gain) {
        params->manual_analog_gain = *inParams.manual_analog_gain;
    }

    if (base->manual_iso) {
        params->manual_iso = *inParams.manual_iso;
    }

    if (base->manual_limits) {
        params->manual_limits = *inParams.manual_limits;
    }

    return true;
}

bool IPCAiq::clientUnflattenAe(const ae_run_params& params, ia_aiq_ae_results** results)
{
    LOG1("@%s, results:%p", __FUNCTION__, results);
    CheckError((results == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    ae_run_params_results* res = const_cast<ae_run_params_results*>(&params.res);
    bool ret = unflattenAeResults(res);
    CheckError((ret == false), false, "@%s, unflattenAeResults fails", __FUNCTION__);

    *results = &res->base;

    return true;
}

bool IPCAiq::serverUnflattenAe(const ae_run_params& inParams, ia_aiq_ae_input_params** params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    ia_aiq_ae_input_params* base = const_cast<ia_aiq_ae_input_params*>(&inParams.base);
    if (base->aec_features) {
        base->aec_features = const_cast<ia_aiq_ae_features*>(&inParams.aec_features);
    }

    if (base->exposure_coordinate) {
        base->exposure_coordinate = const_cast<ia_coordinate*>(&inParams.exposure_coordinate);
    }

    if (base->exposure_window) {
        base->exposure_window = const_cast<ia_rectangle*>(&inParams.exposure_window);
    }

    if (base->sensor_descriptor) {
        base->sensor_descriptor = const_cast<ia_aiq_exposure_sensor_descriptor*>(&inParams.sensor_descriptor);
    }

    if (base->manual_exposure_time_us) {
        base->manual_exposure_time_us = const_cast<long*>(&inParams.manual_exposure_time_us);
    }

    if (base->manual_analog_gain) {
        base->manual_analog_gain = const_cast<float*>(&inParams.manual_analog_gain);
    }

    if (base->manual_iso) {
        base->manual_iso = const_cast<short*>(&inParams.manual_iso);
    }

    if (base->manual_limits) {
        base->manual_limits = const_cast<ia_aiq_ae_manual_limits*>(&inParams.manual_limits);
    }

    *params = base;

    return true;
}

bool IPCAiq::serverFlattenAe(const ia_aiq_ae_results& aeResults, ae_run_params* params)
{
    LOG1("@%s, results:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    bool ret = flattenAeResults(aeResults, &params->res);
    CheckError((ret == false), false, "@%s, flattenAeResults fails", __FUNCTION__);

    return true;
}

bool IPCAiq::flattenAeResults(const ia_aiq_ae_results& aeResults, ae_run_params_results* res)
{
    LOG2("@%s, res:%p", __FUNCTION__, res);
    CheckError((res == nullptr), false, "@%s, res is nullptr", __FUNCTION__);

    res->base = aeResults;
    const ia_aiq_ae_results* base = &res->base;

    if (base->exposures) {
        res->exposures[0] = *aeResults.exposures;

        if (res->exposures[0].exposure) {
            res->exposure = *aeResults.exposures->exposure;
        }
        if (res->exposures[0].sensor_exposure) {
            res->sensor_exposure = *aeResults.exposures->sensor_exposure;
        }
        if (res->exposures[0].exposure_plan_ids) {
            res->exposure_plan_ids[0] = *aeResults.exposures->exposure_plan_ids;
        }
    }

    if (base->weight_grid) {
        res->weight_grid = *aeResults.weight_grid;

        if (res->weight_grid.weights) {
            unsigned int gridElements =
                aeResults.weight_grid->width * aeResults.weight_grid->height;
            gridElements = CLIP(gridElements, MAX_SIZE_WEIGHT_GRID, 1);
            MEMCPY_S(res->weights, sizeof(res->weights),
                aeResults.weight_grid->weights, gridElements * sizeof(unsigned char));
        }
    }

    if (base->flashes) {
        // Valgrind will give warning from here in the first round. It should be fine.
        if (aeResults.num_flashes > 0) {
            MEMCPY_S(res->flashes, sizeof(res->flashes),
                aeResults.flashes, MAX_NUM_FLASHES * sizeof(ia_aiq_flash_parameters));
        }
    }

    if (base->aperture_control) {
        res->aperture_control = *aeResults.aperture_control;
    }

    return true;
}

bool IPCAiq::unflattenAeResults(ae_run_params_results* res)
{
    LOG2("@%s, res:%p", __FUNCTION__, res);
    CheckError((res == nullptr), false, "@%s, res is nullptr", __FUNCTION__);

    ia_aiq_ae_results* base = &res->base;

    if (base->exposures) {
        base->exposures = const_cast<ia_aiq_ae_exposure_result*>(res->exposures);
        if (base->exposures->exposure) {
            base->exposures->exposure = const_cast<ia_aiq_exposure_parameters*>(&res->exposure);
        }
        if (base->exposures->sensor_exposure) {
            base->exposures->sensor_exposure = const_cast<ia_aiq_exposure_sensor_parameters*>(&res->sensor_exposure);
        }
        if (base->exposures->exposure_plan_ids) {
            base->exposures->exposure_plan_ids = const_cast<unsigned int*>(res->exposure_plan_ids);
        }
    }

    if (base->weight_grid) {
        base->weight_grid = const_cast<ia_aiq_hist_weight_grid*>(&res->weight_grid);
        if (base->weight_grid->weights) {
            base->weight_grid->weights = const_cast<unsigned char*>(res->weights);
        }
    }

    if (base->flashes) {
        base->flashes = const_cast<ia_aiq_flash_parameters*>(res->flashes);
    }

    if (base->aperture_control) {
        base->aperture_control = const_cast<ia_aiq_aperture_control*>(&res->aperture_control);
    }

    return true;
}

// awb
bool IPCAiq::clientFlattenAwb(uintptr_t aiq, const ia_aiq_awb_input_params& inParams, awb_run_params* params)
{
    LOG1("@%s, aiq:0x%" PRIxPTR ", params:%p", __FUNCTION__, aiq, params);
    CheckError(reinterpret_cast<ia_aiq*>(aiq) == nullptr, false, "@%s, aiq is nullptr", __FUNCTION__);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    params->aiq_handle = aiq;

    params->base = inParams;
    const ia_aiq_awb_input_params* base = &params->base;

    if (base->manual_cct_range) {
        params->manual_cct_range = *inParams.manual_cct_range;
    }

    if (base->manual_white_coordinate) {
        params->manual_white_coordinate = *inParams.manual_white_coordinate;
    }

    return true;
}

bool IPCAiq::clientUnflattenAwb(const awb_run_params& inParams, ia_aiq_awb_results** results)
{
    LOG1("@%s, results:%p", __FUNCTION__, results);
    CheckError((results == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    *results = const_cast<ia_aiq_awb_results*>(&inParams.results);

    return true;
}

bool IPCAiq::serverUnflattenAwb(const awb_run_params& inParams, ia_aiq_awb_input_params** params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    ia_aiq_awb_input_params* base = const_cast<ia_aiq_awb_input_params*>(&inParams.base);

    if (base->manual_cct_range) {
        base->manual_cct_range = const_cast<ia_aiq_awb_manual_cct_range*>(&inParams.manual_cct_range);
    }

    if (base->manual_white_coordinate) {
        base->manual_white_coordinate = const_cast<ia_coordinate*>(&inParams.manual_white_coordinate);
    }

    LOG1("@%s, manual_cct_range:%p, manual_white_coordinate:%p",
        __FUNCTION__, base->manual_cct_range, base->manual_white_coordinate);

    *params = base;

    return true;
}

bool IPCAiq::serverFlattenAwb(const ia_aiq_awb_results& awbResults, awb_run_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    ia_aiq_awb_results* results = &params->results;
    *results = awbResults;

    LOG2("awb results->accurate_r_per_g:%f", results->accurate_r_per_g);
    LOG2("awb results->accurate_b_per_g:%f", results->accurate_b_per_g);
    LOG2("awb results->final_r_per_g:%f", results->final_r_per_g);
    LOG2("awb results->final_b_per_g:%f", results->final_b_per_g);
    LOG2("awb results->cct_estimate:%d", results->cct_estimate);
    LOG2("awb results->distance_from_convergence:%f", results->distance_from_convergence);

    return true;
}

// pa
bool IPCAiq::clientFlattenPa(uintptr_t aiq, const ia_aiq_pa_input_params& inParams, pa_run_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(reinterpret_cast<ia_aiq*>(aiq) == nullptr, false, "@%s, aiq is nullptr", __FUNCTION__);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    params->aiq_handle = aiq;

    params->base = inParams;
    ia_aiq_pa_input_params* base = &params->base;

    if (base->awb_results) {
        params->awb_results = *base->awb_results;
    }

    if (base->exposure_params) {
        params->exposure_params = *base->exposure_params;
    }

    if (base->color_gains) {
        params->color_gains = *base->color_gains;
    }

    return true;
}

bool IPCAiq::clientUnflattenPa(const pa_run_params& params, ia_aiq_pa_results** results)
{
    LOG1("@%s, results:%p", __FUNCTION__, results);
    CheckError((results == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    pa_run_params_results* res = const_cast<pa_run_params_results*>(&params.res);
    bool ret = unflattenPaResults(res);
    CheckError((ret == false), false, "@%s, unflattenPaResults fails", __FUNCTION__);

    *results = &res->base;

    return true;
}

bool IPCAiq::serverUnflattenPa(const pa_run_params& inParams, ia_aiq_pa_input_params** params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    ia_aiq_pa_input_params* base = const_cast<ia_aiq_pa_input_params*>(&inParams.base);

    if (base->awb_results) {
        base->awb_results = const_cast<ia_aiq_awb_results*>(&inParams.awb_results);
    }

    if (base->exposure_params) {
        base->exposure_params = const_cast<ia_aiq_exposure_parameters*>(&inParams.exposure_params);
    }

    if (base->color_gains) {
        base->color_gains = const_cast<ia_aiq_color_channels*>(&inParams.color_gains);
    }

    *params = base;

    return true;
}

bool IPCAiq::serverFlattenPa(const ia_aiq_pa_results& paResults, pa_run_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    bool ret = flattenPaResults(paResults, &params->res);
    CheckError((ret == false), false, "@%s, flattenPaResults fails", __FUNCTION__);

    return true;
}

bool IPCAiq::flattenPaResults(const ia_aiq_pa_results& paResults, pa_run_params_results* res)
{
    LOG2("@%s, res:%p", __FUNCTION__, res);
    CheckError((res == nullptr), false, "@%s, res is nullptr", __FUNCTION__);

    res->base = paResults;
    ia_aiq_pa_results* base = &res->base;

    ia_aiq_color_channels_lut* linearization = &base->linearization;
    CheckError((MAX_NUM_LUTS < linearization->size), false,
        "@%s, linearization:%d is too big", __FUNCTION__, linearization->size);
    if (linearization->gr) {
        MEMCPY_S(res->gr, sizeof(res->gr),
            linearization->gr, sizeof(*linearization->gr) * linearization->size);
    }
    if (linearization->r) {
        MEMCPY_S(res->r, sizeof(res->r),
            linearization->r, sizeof(*linearization->r) * linearization->size);
    }
    if (linearization->b) {
        MEMCPY_S(res->b, sizeof(res->b),
            linearization->b, sizeof(*linearization->b) * linearization->size);
    }
    if (linearization->gb) {
        MEMCPY_S(res->gb, sizeof(res->gb),
            linearization->gb, sizeof(*linearization->gb) * linearization->size);
    }

    ia_aiq_advanced_ccm_t* preferred_acm = base->preferred_acm;
    if (preferred_acm) {
        CheckError((MAX_SECTOR_COUNT < preferred_acm->sector_count), false,
            "@%s, sector_count:%d is too big", __FUNCTION__, preferred_acm->sector_count);

        res->preferred_acm = *preferred_acm;

        if (preferred_acm->hue_of_sectors) {
            MEMCPY_S(res->hue_of_sectors, sizeof(res->hue_of_sectors),
                preferred_acm->hue_of_sectors,
                sizeof(*preferred_acm->hue_of_sectors) * preferred_acm->sector_count);
        }

        if (preferred_acm->advanced_color_conversion_matrices) {
            MEMCPY_S(res->advanced_color_conversion_matrices, sizeof(res->advanced_color_conversion_matrices),
                preferred_acm->advanced_color_conversion_matrices,
                sizeof(*preferred_acm->advanced_color_conversion_matrices) * preferred_acm->sector_count);
        }
    }

    ia_aiq_ir_weight_t* ir_weight = base->ir_weight;
    if (ir_weight) {
        res->ir_weight = *ir_weight;

        if(ir_weight->ir_weight_grid_R) {
            MEMCPY_S(res->ir_weight_grid_R, sizeof(res->ir_weight_grid_R),
                ir_weight->ir_weight_grid_R,
                sizeof(*ir_weight->ir_weight_grid_R) * ir_weight->height * ir_weight->width);
        }

        if(ir_weight->ir_weight_grid_G) {
            MEMCPY_S(res->ir_weight_grid_G, sizeof(res->ir_weight_grid_G),
                ir_weight->ir_weight_grid_G,
                sizeof(*ir_weight->ir_weight_grid_G) * ir_weight->height * ir_weight->width);
        }

        if(ir_weight->ir_weight_grid_B) {
            MEMCPY_S(res->ir_weight_grid_B, sizeof(res->ir_weight_grid_B),
                ir_weight->ir_weight_grid_B,
                sizeof(*ir_weight->ir_weight_grid_B) * ir_weight->height * ir_weight->width);
        }
    }

    return true;
}

bool IPCAiq::unflattenPaResults(pa_run_params_results* res)
{
    LOG2("@%s, res:%p", __FUNCTION__, res);
    CheckError((res == nullptr), false, "@%s, res is nullptr", __FUNCTION__);

    ia_aiq_pa_results* base = const_cast<ia_aiq_pa_results*>(&res->base);

    if (base->linearization.gr) {
        base->linearization.gr = const_cast<float*>(res->gr);
    }
    if (base->linearization.r) {
        base->linearization.r = const_cast<float*>(res->r);
    }
    if (base->linearization.b) {
        base->linearization.b = const_cast<float*>(res->b);
    }
    if (base->linearization.gb) {
        base->linearization.gb = const_cast<float*>(res->gb);
    }

    if (base->preferred_acm) {
        base->preferred_acm = const_cast<ia_aiq_advanced_ccm_t*>(&res->preferred_acm);

        if (base->preferred_acm->hue_of_sectors) {
            base->preferred_acm->hue_of_sectors =
                const_cast<unsigned int*>(res->hue_of_sectors);
        }

        if (base->preferred_acm->advanced_color_conversion_matrices) {
            base->preferred_acm->advanced_color_conversion_matrices =
                const_cast<float(*)[3][3]>(res->advanced_color_conversion_matrices);
        }
    }

    if (base->ir_weight) {
        base->ir_weight = const_cast<ia_aiq_ir_weight_t*>(&res->ir_weight);

        if (base->ir_weight->ir_weight_grid_R) {
            base->ir_weight->ir_weight_grid_R =
                const_cast<unsigned short*>(res->ir_weight_grid_R);
        }
        if (base->ir_weight->ir_weight_grid_G) {
            base->ir_weight->ir_weight_grid_G =
                const_cast<unsigned short*>(res->ir_weight_grid_G);
        }
        if (base->ir_weight->ir_weight_grid_B) {
            base->ir_weight->ir_weight_grid_B =
                const_cast<unsigned short*>(res->ir_weight_grid_B);
        }
    }

    return true;
}


// sa
bool IPCAiq::clientFlattenSa(uintptr_t aiq, const ia_aiq_sa_input_params& inParams, sa_run_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(reinterpret_cast<ia_aiq*>(aiq) == nullptr, false, "@%s, aiq is nullptr", __FUNCTION__);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    params->aiq_handle = aiq;

    params->base = inParams;
    const ia_aiq_sa_input_params* base = &params->base;

    if (base->sensor_frame_params) {
        params->sensor_frame_params = *inParams.sensor_frame_params;
    }

    if (base->awb_results) {
        params->awb_results = *inParams.awb_results;
    }

    return true;
}

bool IPCAiq::clientUnflattenSa(const sa_run_params& params, ia_aiq_sa_results** results)
{
    LOG1("@%s, results:%p", __FUNCTION__, results);
    CheckError((results == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    sa_run_params_results* res = const_cast<sa_run_params_results*>(&params.res);
    bool ret = unflattenSaResults(res);
    CheckError((ret == false), false, "@%s, unflattenSaResults fails", __FUNCTION__);

    *results = &res->base;

    return true;
}

bool IPCAiq::serverUnflattenSa(const sa_run_params& inParams, ia_aiq_sa_input_params** params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    ia_aiq_sa_input_params* base = const_cast<ia_aiq_sa_input_params*>(&inParams.base);

    if (base->sensor_frame_params) {
        base->sensor_frame_params = const_cast<ia_aiq_frame_params*>(&inParams.sensor_frame_params);
    }

    if (base->awb_results) {
        base->awb_results = const_cast<ia_aiq_awb_results*>(&inParams.awb_results);
    }

    *params = base;

    return true;
}

bool IPCAiq::serverFlattenSa(const ia_aiq_sa_results& saResults, sa_run_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, results is nullptr", __FUNCTION__);

    bool ret = flattenSaResults(saResults, &params->res);
    CheckError((ret == false), false, "@%s, flattenSaResults fails", __FUNCTION__);

    return true;
}

bool IPCAiq::flattenSaResults(const ia_aiq_sa_results& saResults, sa_run_params_results* res)
{
    LOG2("@%s, res:%p", __FUNCTION__, res);
    CheckError((res == nullptr), false, "@%s, res is nullptr", __FUNCTION__);

    res->base = saResults;
    ia_aiq_sa_results* base = &res->base;

    LOG2("sa_results: width:%d, height:%d, lsc_update:%d",
        base->width, base->height, base->lsc_update);
    LOG2("sa_results: channel_gr:%p, channel_r:%p, channel_b:%p, channel_gb:%p",
        base->channel_gr, base->channel_r, base->channel_b, base->channel_gb);

    if (base->width && base->height) {
        size_t size = base->width * base->height * sizeof(*base->channel_r);

        if (base->channel_gr) {
            MEMCPY_S(res->channel_gr, sizeof(res->channel_gr),
                base->channel_gr, size);
        }

        if (base->channel_r) {
            MEMCPY_S(res->channel_r, sizeof(res->channel_r),
                base->channel_r, size);
        }

        if (base->channel_b) {
            MEMCPY_S(res->channel_b, sizeof(res->channel_b),
                base->channel_b, size);
        }

        if (base->channel_gb) {
            MEMCPY_S(res->channel_gb, sizeof(res->channel_gb),
                base->channel_gb, size);
        }
    } else if (base->lsc_update) {
        LOGE("@%s, Error: LSC table size is 0", __FUNCTION__);
    }

    return true;
}

bool IPCAiq::unflattenSaResults(sa_run_params_results* res)
{
    LOG2("@%s, res:%p", __FUNCTION__, res);
    CheckError((res == nullptr), false, "@%s, res is nullptr", __FUNCTION__);

    ia_aiq_sa_results* base = &res->base;

    LOG2("sa_results_data:height:%d, width:%d", base->height, base->width);

    if (base->channel_gr) {
        base->channel_gr = const_cast<float*>(res->channel_gr);
    }
    if (base->channel_r) {
        base->channel_r = const_cast<float*>(res->channel_r);
    }
    if (base->channel_b) {
        base->channel_b = const_cast<float*>(res->channel_b);
    }
    if (base->channel_gb) {
        base->channel_gb = const_cast<float*>(res->channel_gb);
    }

    return true;
}

} /* namespace intel */
} /* namespace cros */
