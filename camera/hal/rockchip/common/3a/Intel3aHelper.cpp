/*
 * Copyright (C) 2017 Intel Corporation
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

#define LOG_TAG "Intel3aHelper"

#include "Intel3aHelper.h"

#include <utils/Errors.h>
#include <math.h>
#include <ia_cmc_parser.h>
#include <ia_exc.h>
#include <ia_log.h>
#include <ia_mkn_encoder.h>
#include "LogHelper.h"

NAMESPACE_DECLARATION {

/****************************************************************************
 *  DEBUGGING TOOLS FOR DUMPING AIQ STRUCTURES
 */
void
Intel3aHelper::dumpStatInputParams(const ia_aiq_statistics_input_params* sip)
{
    LOGAIQ("Stats Input Params id:%lld ts: %lld", sip->frame_id,
                                                sip->frame_timestamp);

    LOGAIQ("Stats Input Params frame_ae_parameters %p", sip->frame_ae_parameters);
    if (sip->frame_ae_parameters) {
        dumpAeResult(sip->frame_ae_parameters);
    } else {
        LOGAIQ("nullptr pointer in Stats Input Params frame_ae_parameters");
    }
    LOGAIQ("Stats Input Params  frame_af_parameters %p", sip->frame_af_parameters);
    if (sip->frame_af_parameters) {
        dumpAfResult(sip->frame_af_parameters);
    } else {
        LOGAIQ("nullptr pointer in Stats Input Params frame_af_parameters");
    }
    LOGAIQ("AF grid array %p", sip->af_grids);
    if (sip->af_grids) {
        LOGAIQ("AF grid 0 %p", sip->af_grids[0]);
    }

    LOGAIQ("RGBS grid array %p number %d", sip->rgbs_grids, sip->num_rgbs_grids);
    if (sip->rgbs_grids) {
        dumpRGBSGrids(sip->rgbs_grids, sip->num_rgbs_grids);
    }else {
        LOGAIQ("No RGBS Grids!!");
    }

    LOGAIQ("Stats Input Params: orientation %d ", sip->camera_orientation);
    LOGAIQ("Stats Input Params: awb_results %p ", sip->awb_results);

}

void
Intel3aHelper::dumpAeInputParams(const ia_aiq_ae_input_params &aeInput)
{
    LOGAIQ("AE Input: num_exposures %d", aeInput.num_exposures);
    LOGAIQ("AE Input: frame use: %s",
            aeInput.frame_use == ia_aiq_frame_use_preview ? "preview" :
            aeInput.frame_use == ia_aiq_frame_use_still ? "still" :
            aeInput.frame_use == ia_aiq_frame_use_continuous ? "cont" : "video");
    LOGAIQ("AE Input: flash_mode: %s",
            aeInput.flash_mode == ia_aiq_flash_mode_auto ? "auto" :
            aeInput.flash_mode == ia_aiq_flash_mode_on ? "on" : "off");
    LOGAIQ("AE Input: operation_mode: %s",
            aeInput.operation_mode == ia_aiq_ae_operation_mode_automatic ? "auto" :
            aeInput.operation_mode == ia_aiq_ae_operation_mode_long_exposure ? "long exp" :
            aeInput.operation_mode == ia_aiq_ae_operation_mode_action ? "action" :
            aeInput.operation_mode == ia_aiq_ae_operation_mode_video_conference ? "video conf" :
            aeInput.operation_mode == ia_aiq_ae_operation_mode_production_test ? "prod test" :
            aeInput.operation_mode == ia_aiq_ae_operation_mode_ultra_low_light ? "ULL" :
            aeInput.operation_mode == ia_aiq_ae_operation_mode_hdr ? "HDR" : "custom");
    LOGAIQ("AE Input: metering_mode: %s",
            aeInput.metering_mode == ia_aiq_ae_metering_mode_evaluative ? "eval" : "center");
    LOGAIQ("AE Input: priority_mode: %s",
            aeInput.priority_mode == ia_aiq_ae_priority_mode_normal ? "normal" :
            aeInput.priority_mode == ia_aiq_ae_priority_mode_highlight ? "highlight" : "shadow");
    LOGAIQ("AE Input: flicker_reduction_mode: %s",
            aeInput.flicker_reduction_mode == ia_aiq_ae_flicker_reduction_detect ? "detect" :
            aeInput.flicker_reduction_mode == ia_aiq_ae_flicker_reduction_auto ? "auto" :
            aeInput.flicker_reduction_mode == ia_aiq_ae_flicker_reduction_50hz ? "50Hz" :
            aeInput.flicker_reduction_mode == ia_aiq_ae_flicker_reduction_60hz ? "60Hz" : "off");

    if (aeInput.manual_limits)
        LOGAIQ("Manual controls: exp time [%d-%d] frametime [%d-%d] iso [%d-%d]",
            aeInput.manual_limits->manual_exposure_time_min,
            aeInput.manual_limits->manual_exposure_time_max,
            aeInput.manual_limits->manual_frame_time_us_min,
            aeInput.manual_limits->manual_frame_time_us_max,
            aeInput.manual_limits->manual_iso_min,
            aeInput.manual_limits->manual_iso_max);
}

void
Intel3aHelper::dumpAeResult(const ia_aiq_ae_results* aeResult)
{
    if (aeResult->exposures) {
        if (aeResult->exposures[0].exposure) {
            LOGAIQ(" AE exp result ag %2.1f exp time %d iso %d",
                    aeResult->exposures[0].exposure->analog_gain,
                    aeResult->exposures[0].exposure->exposure_time_us,
                    aeResult->exposures[0].exposure->iso);
        }
        if (aeResult->exposures[0].sensor_exposure) {
            LOGAIQ("AE sensor exp result ag %d coarse int time %d fine: %d llp:%d fll:%d",
                    aeResult->exposures[0].sensor_exposure->analog_gain_code_global,
                    aeResult->exposures[0].sensor_exposure->coarse_integration_time,
                    aeResult->exposures[0].sensor_exposure->fine_integration_time,
                    aeResult->exposures[0].sensor_exposure->line_length_pixels,
                    aeResult->exposures[0].sensor_exposure->frame_length_lines);
        }
        LOGAIQ("Converged : %s", aeResult->exposures[0].converged? "YES": "NO");
    } else {
        LOGAIQ("nullptr pointer in StatsInputParams->frame_ae_parameters->exposures");
    }
}

void
Intel3aHelper::dumpAwbResult(const ia_aiq_awb_results* awbResult)
{
    if (awbResult) {
        LOGAIQ("AWB result: accurate_r/g %f, accurate_b/g %f final_r/g %f final_b/g %f",
                awbResult->accurate_r_per_g,
                awbResult->accurate_b_per_g,
                awbResult->final_r_per_g,
                awbResult->final_b_per_g);
        LOGAIQ("AWB result: cct_estimate %d, distance_from_convergence %f",
                awbResult->cct_estimate,
                awbResult->distance_from_convergence);
    } else {
        LOGAIQ("nullptr pointer in passed, cannot dump");
    }
}

void
Intel3aHelper::dumpAfResult(const ia_aiq_af_results* afResult)
{
    if (afResult == nullptr)
        return;

    LOGAIQ("AF results current_focus_distance %d final_position_reached %s",
                afResult->current_focus_distance,
                afResult->final_lens_position_reached ? "TRUE":"FALSE");
    LOGAIQ("AF results driver_action %d, next_lens_position %d",
                afResult->lens_driver_action,
                afResult->next_lens_position);
    LOGAIQ("AF results use_af_assist %s",
          afResult->use_af_assist? "TRUE":"FALSE");

    switch (afResult->status) {
    case ia_aiq_af_status_local_search:
        LOGAIQ("AF result state _local_search");
        break;
    case ia_aiq_af_status_extended_search:
        LOGAIQ("AF result state extended_search");
        break;
    case ia_aiq_af_status_success:
        LOGAIQ("AF state success");
        break;
    case ia_aiq_af_status_fail:
        LOGAIQ("AF state fail");
        break;
    case ia_aiq_af_status_idle:
    default:
        LOGAIQ("AF state idle");
    }
}

void
Intel3aHelper::dumpAfInputParams(const ia_aiq_af_input_params* afCfg)
{
    if (afCfg == nullptr)
        return;

    LOGAIQ("AF input params flash_mode %d", afCfg->flash_mode);
    LOGAIQ("AF input params focus_metering_mode %d", afCfg->focus_metering_mode);
    LOGAIQ("AF input params focus_mode %d", afCfg->focus_mode);
    LOGAIQ("AF input params focus_range %d", afCfg->focus_range);
    LOGAIQ("AF input params frame_use %d", afCfg->frame_use);
    LOGAIQ("AF input params lens_position %d", afCfg->lens_position);
    LOGAIQ("AF input params lens_movement_start_timestamp %lld",
                            afCfg->lens_movement_start_timestamp);

    if (afCfg->manual_focus_parameters) {
        LOGAIQ("AF Input params manual_focus_distance %d manual_focus_action %d",
                afCfg->manual_focus_parameters->manual_focus_distance,
                afCfg->manual_focus_parameters->manual_focus_action);
    }
}

void
Intel3aHelper::dumpPaResult(const ia_aiq_pa_results* paResult)
{

    LOGAIQ("   PA results brightness %f saturation %f",
            paResult->brightness_level,
            paResult->saturation_factor);
    LOGAIQ("   PA results black level %f %f %f  %f ",
            paResult->black_level.r,
            paResult->black_level.gr,
            paResult->black_level.gb,
            paResult->black_level.b);
    LOGAIQ("   PA results color gains %f %f %f  %f ",
                paResult->color_gains.r,
                paResult->color_gains.gr,
                paResult->color_gains.gb,
                paResult->color_gains.b);
    LOGAIQ("   PA results linearization table size %d",
                    paResult->linearization.size);

    for(int i = 0; i< MAX_COLOR_CONVERSION_MATRIX; i++) {
        LOGAIQ("   PA results color matrix  [%.3f %.3f %.3f] ",
                paResult->color_conversion_matrix[i][0],
                paResult->color_conversion_matrix[i][1],
                paResult->color_conversion_matrix[i][2]);
    }
}

void
Intel3aHelper::dumpSaResult(const ia_aiq_sa_results* saResult)
{
    LOGAIQ("   SA results lsc Update %d size %dx%d",
            saResult->lsc_update,
            saResult->width,
            saResult->height);
}

void
Intel3aHelper::dumpRGBSGrids(const ia_aiq_rgbs_grid **rgbs_grids, int gridCount)
{
    for (int i = 0; i< gridCount; i++) {
        if (rgbs_grids[i]) {
            LOGAIQ("GRID %d - width %d height %d", i, rgbs_grids[i]->grid_width,
                                                 rgbs_grids[i]->grid_height);
            //TODO print the grid
        }
    }
}

} NAMESPACE_DECLARATION_END
