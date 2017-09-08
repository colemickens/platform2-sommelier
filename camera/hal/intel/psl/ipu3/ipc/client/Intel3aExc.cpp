/*
 * Copyright 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "IA_AIQ_EXC"

#include "Intel3aExc.h"
#include "LogHelper.h"
#include "Intel3AClient.h"
#include "UtilityMacros.h"

NAMESPACE_DECLARATION {
Intel3aExc::Intel3aExc():
    mInitialized(false)
{
    LOG1("@%s", __FUNCTION__);

    mMems = {
        {"/cmcGainToUnitsShm", sizeof(ia_exc_analog_gain_to_sensor_units_params), &mMemGainToSensor, false},
        {"/cmcUnitsToGainShm", sizeof(ia_exc_analog_gain_to_sensor_units_params), &mMemSensorToGain, false}};

    bool success = mCommon.allocateAllShmMems(mMems);
    if (!success) {
        mCommon.releaseAllShmMems(mMems);
        return;
    }

    LOG1("@%s, done", __FUNCTION__);
    mInitialized = true;
}

Intel3aExc::~Intel3aExc()
{
    LOG1("@%s", __FUNCTION__);
    mCommon.releaseAllShmMems(mMems);
}

ia_err Intel3aExc::AnalogGainToSensorUnits(
    const cmc_parsed_analog_gain_conversion_t* gain_conversion,
    float analog_gain,
    unsigned short* analog_gain_code)
{
    LOG1("@%s, gain_conversion:%p, analog_gain:%f, analog_gain_code:%p",
        __FUNCTION__, gain_conversion, analog_gain, analog_gain_code);
    CheckError((gain_conversion == nullptr), ia_err_argument, "@%s, gain_conversion is nullptr", __FUNCTION__);
    CheckError((analog_gain_code == nullptr), ia_err_argument, "@%s, analog_gain_code is nullptr", __FUNCTION__);
    CheckError(mInitialized == false, ia_err_none, "@%s, mInitialized is false", __FUNCTION__);

    ia_exc_analog_gain_to_sensor_units_params* params =
        static_cast<ia_exc_analog_gain_to_sensor_units_params*>(mMemGainToSensor.mAddr);

    bool ret = mIpc.clientFlattenGainToSensor(*gain_conversion, analog_gain, params);
    CheckError(ret == false, ia_err_general, "@%s, clientFlattenGainToSensor fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_EXC_ANALOG_GAIN_TO_SENSOR, mMemGainToSensor.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    *analog_gain_code = params->results.code;

    return ia_err_none;
}

ia_err Intel3aExc::SensorUnitsToAnalogGain(
    const cmc_parsed_analog_gain_conversion_t* gain_conversion,
    unsigned short gain_code,
    float* analog_gain)
{
    LOG1("@%s, gain_conversion:%p, gain_code:%d, analog_gain:%f",
        __FUNCTION__, gain_conversion, gain_code, analog_gain);

    CheckError((gain_conversion == nullptr), ia_err_argument, "@%s, gain_conversion is nullptr", __FUNCTION__);
    CheckError((analog_gain == nullptr), ia_err_argument, "@%s, analog_gain is nullptr", __FUNCTION__);
    CheckError(mInitialized == false, ia_err_none, "@%s, mInitialized is false", __FUNCTION__);

    ia_exc_analog_gain_to_sensor_units_params* params =
        static_cast<ia_exc_analog_gain_to_sensor_units_params*>(mMemSensorToGain.mAddr);

    bool ret = mIpc.clientFlattenSensorToGain(*gain_conversion, gain_code, params);
    CheckError(ret == false, ia_err_general, "@%s, clientFlattenSensorToGain fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_EXC_SENSOR_TO_ANALOG_GAIN, mMemSensorToGain.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    *analog_gain = params->results.value;

    return ia_err_none;
}

} NAMESPACE_DECLARATION_END
