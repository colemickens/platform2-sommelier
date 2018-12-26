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
#define LOG_TAG "ExcLibrary"

#include "ExcLibrary.h"

#include "LogHelper.h"

using namespace android::camera2;

namespace intel {
namespace camera {

ExcLibrary::ExcLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

ExcLibrary::~ExcLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

status_t ExcLibrary::analog_gain_to_sensor_units(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(ia_exc_analog_gain_to_sensor_units_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    ia_exc_analog_gain_to_sensor_units_params* params = static_cast<ia_exc_analog_gain_to_sensor_units_params*>(pData);
    cmc_parsed_analog_gain_conversion_t* iaLibInput = nullptr;

    bool ret = mIpc.serverUnflattenGainToSensor(*params, &iaLibInput);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflattenGainToSensor fails", __FUNCTION__);

    ia_err err = ia_exc_analog_gain_to_sensor_units(iaLibInput,
                                       params->input.value,
                                       &params->results.code);
    CheckError(err != ia_err_none, ia_err_general, "@%s, call ia_exc_analog_gain_to_sensor_units() fails", __FUNCTION__);
    LOG2("@%s, ia_exc_analog_gain_to_sensor_units return:%d", __FUNCTION__, err);

    return err == ia_err_none ? NO_ERROR : UNKNOWN_ERROR;
}

status_t ExcLibrary::sensor_units_to_analog_gain(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(ia_exc_analog_gain_to_sensor_units_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    ia_exc_analog_gain_to_sensor_units_params* params = static_cast<ia_exc_analog_gain_to_sensor_units_params*>(pData);
    cmc_parsed_analog_gain_conversion_t* iaLibInput = nullptr;

    bool ret = mIpc.serverUnflattenSensorToGain(*params, &iaLibInput);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflattenSensorToGain fails", __FUNCTION__);

    ia_err err = ia_exc_sensor_units_to_analog_gain(iaLibInput,
                                       params->input.code,
                                       &params->results.value);
    CheckError(err != ia_err_none, ia_err_general, "@%s, call ia_exc_sensor_units_to_analog_gain() fails", __FUNCTION__);
    LOG2("@%s, ia_exc_sensor_units_to_analog_gain return:%d", __FUNCTION__, err);

    return err == ia_err_none ? NO_ERROR : UNKNOWN_ERROR;
}

} // namespace camera
} // namespace intel
