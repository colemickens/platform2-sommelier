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

#ifndef PSL_IPU3_IPC_IPCEXC_H_
#define PSL_IPU3_IPC_IPCEXC_H_
#include <ia_exc.h>
#include <ia_types.h>
#include <ia_aiq_types.h>
#include <ia_cmc_types.h>
#include "IPCCmc.h"

namespace cros {
namespace intel {
struct ia_exc_analog_gain_to_sensor_units_params {
    cmc_parsed_analog_gain_conversion_t base;
    cmc_parsed_analog_gain_conversion_data gain_conversion;

    union analog_gain {
        float value;
        unsigned short code;
    };
    analog_gain input;
    analog_gain results;
};

class IPCExc {
public:
    IPCExc();
    virtual ~IPCExc();

    // for AnalogGainToSensorUnits
    bool clientFlattenGainToSensor(const cmc_parsed_analog_gain_conversion_t& gain_conversion,
                             float gain,
                             ia_exc_analog_gain_to_sensor_units_params* params);
    bool serverUnflattenGainToSensor(ia_exc_analog_gain_to_sensor_units_params& params,
                             cmc_parsed_analog_gain_conversion_t** libInput);

    // for SensorUnitsToAnalogGain
    bool clientFlattenSensorToGain(const cmc_parsed_analog_gain_conversion_t& gain_conversion,
                             unsigned short gain_code,
                             ia_exc_analog_gain_to_sensor_units_params* params);
    bool serverUnflattenSensorToGain(ia_exc_analog_gain_to_sensor_units_params& params,
                             cmc_parsed_analog_gain_conversion_t** libInput);
};
} /* namespace intel */
} /* namespace cros */
#endif //PSL_IPU3_IPC_IPCEXC_H_
