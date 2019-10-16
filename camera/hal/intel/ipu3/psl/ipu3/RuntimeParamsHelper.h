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

#ifndef PSL_IPU3_RUNTIMEPARAMSHELPER_H_
#define PSL_IPU3_RUNTIMEPARAMSHELPER_H_

#include <IPU3AICCommon.h>
#include "utils/Errors.h"

namespace cros {
namespace intel {

class RuntimeParamsHelper {
public:
    static status_t allocateAiStructs(IPU3AICRuntimeParams &runtimeParams);
    static void deleteAiStructs(IPU3AICRuntimeParams &runtimeParams);
    static void copyPaResults(IPU3AICRuntimeParams &to, ia_aiq_pa_results &from);
    static void copySaResults(IPU3AICRuntimeParams &to, ia_aiq_sa_results &from);
    static void copyWeightGrid(IPU3AICRuntimeParams &to, ia_aiq_hist_weight_grid *from);

private:
    RuntimeParamsHelper();
    virtual ~RuntimeParamsHelper();
};

} /* namespace intel */
} /* namespace cros */

#endif /* PSL_IPU3_RUNTIMEPARAMSHELPER_H_ */
