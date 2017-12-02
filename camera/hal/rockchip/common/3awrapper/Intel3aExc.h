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

#ifndef COMMON_3AWRAPPER_INTEL3AEXC_H_
#define COMMON_3AWRAPPER_INTEL3AEXC_H_
#include <ia_exc.h>

NAMESPACE_DECLARATION {
class Intel3aExc {
public:
    Intel3aExc();
    virtual ~Intel3aExc();

    ia_err AnalogGainToSensorUnits(
              const cmc_parsed_analog_gain_conversion_t* gain_conversion,
              float analog_gain,
              unsigned short* analog_gain_code);

    ia_err SensorUnitsToAnalogGain(
              const cmc_parsed_analog_gain_conversion_t* gain_conversion,
              unsigned short gain_code,
              float* analog_gain);
};
} NAMESPACE_DECLARATION_END
#endif //COMMON_3AWRAPPER_INTEL3AEXC_H_
