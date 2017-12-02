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

#define LOG_TAG "IPC_EXC"

#include "common/UtilityMacros.h"
#include "LogHelper.h"
#include "IPCExc.h"

NAMESPACE_DECLARATION {
IPCExc::IPCExc()
{
    LOG1("@%s", __FUNCTION__);
}

IPCExc::~IPCExc()
{
    LOG1("@%s", __FUNCTION__);
}

static void fillIpcParams(const cmc_parsed_analog_gain_conversion_t& gain_conversion,
                            ia_exc_analog_gain_to_sensor_units_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(params == nullptr, VOID_VALUE, "@%s, params is nullptr", __FUNCTION__);

    params->base = gain_conversion;
    cmc_parsed_analog_gain_conversion_t* base = &params->base;

    // cmc_analog_gain_conversion_t fields
    if (base->cmc_analog_gain_conversion)
        params->gain_conversion.cmc_analog_gain_conversion = *base->cmc_analog_gain_conversion;

    // cmc_analog_gain_segment_t fields
    if (base->cmc_analog_gain_segments)
        MEMCPY_S(&params->gain_conversion.cmc_analog_gain_segments,
                base->cmc_analog_gain_conversion->num_segments * sizeof(cmc_analog_gain_segment_t),
                base->cmc_analog_gain_segments,
                base->cmc_analog_gain_conversion->num_segments * sizeof(cmc_analog_gain_segment_t));

    //cmc_analog_gain_pair_t fields
    if (base->cmc_analog_gain_pairs)
        MEMCPY_S(&params->gain_conversion.cmc_analog_gain_pairs,
                base->cmc_analog_gain_conversion->num_pairs * sizeof(cmc_analog_gain_pair_t),
                base->cmc_analog_gain_pairs,
                base->cmc_analog_gain_conversion->num_pairs * sizeof(cmc_analog_gain_pair_t));
}

static void fillConversionParams(ia_exc_analog_gain_to_sensor_units_params& gain_conversion,
                            cmc_parsed_analog_gain_conversion_t** libInput)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(libInput == nullptr, VOID_VALUE, "@%s, libInput is nullptr", __FUNCTION__);

    cmc_parsed_analog_gain_conversion_t* base = &gain_conversion.base;

    if (base->cmc_analog_gain_conversion)
        base->cmc_analog_gain_conversion =
            (cmc_analog_gain_conversion_t*)&gain_conversion.gain_conversion.cmc_analog_gain_conversion;

    if (base->cmc_analog_gain_pairs)
        base->cmc_analog_gain_pairs =
            (cmc_analog_gain_pair_t*)gain_conversion.gain_conversion.cmc_analog_gain_pairs;

    if (base->cmc_analog_gain_segments)
        base->cmc_analog_gain_segments =
            (cmc_analog_gain_segment_t*)gain_conversion.gain_conversion.cmc_analog_gain_segments;

    *libInput = base;

}

bool IPCExc::clientFlattenGainToSensor(const cmc_parsed_analog_gain_conversion_t& gain_conversion,
                            float gain,
                            ia_exc_analog_gain_to_sensor_units_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    fillIpcParams(gain_conversion, params);

    params->input.value = gain;

    return true;
}

bool IPCExc::serverUnflattenGainToSensor(ia_exc_analog_gain_to_sensor_units_params& params,
                             cmc_parsed_analog_gain_conversion_t** libInput)
{
    LOG1("@%s, libInput:%p", __FUNCTION__, libInput);
    CheckError(libInput == nullptr, false, "@%s, libInput is nullptr", __FUNCTION__);

    fillConversionParams(params, libInput);

    return true;
}

bool IPCExc::clientFlattenSensorToGain(const cmc_parsed_analog_gain_conversion_t& gain_conversion,
                             unsigned short gain_code,
                             ia_exc_analog_gain_to_sensor_units_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    fillIpcParams(gain_conversion, params);

    params->input.code = gain_code;

    return true;

}

bool IPCExc::serverUnflattenSensorToGain(ia_exc_analog_gain_to_sensor_units_params& params,
                             cmc_parsed_analog_gain_conversion_t** libInput)
{
    LOG1("@%s, libInput:%p", __FUNCTION__, libInput);
    CheckError(libInput == nullptr, false, "@%s, libInput is nullptr", __FUNCTION__);

    fillConversionParams(params, libInput);

    return true;
}

} NAMESPACE_DECLARATION_END
