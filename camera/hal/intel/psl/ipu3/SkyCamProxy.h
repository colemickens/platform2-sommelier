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

#ifndef PSL_IPU3_SKYCAMPROXY_H_
#define PSL_IPU3_SKYCAMPROXY_H_

#include "IPU3ISPPipe.h"
#include <string>
#include <ia_cmc_types.h>
#include <ia_types.h>
#include <cpffData.h>

#include <utils/Errors.h>

class ISPPipe;
struct IPU3AICRuntimeParams;

namespace android {
namespace camera2 {

class SkyCamProxy
{
public:

    static std::shared_ptr<SkyCamProxy> createProxy(
            int cameraId, AicMode aicMode, IPU3ISPPipe **pipes, unsigned int numPipes,
            const ia_cmc_t* cmcParsed,
            const ia_binary_data* aiqb,
            IPU3AICRuntimeParams *runtimeParams,
            unsigned int dumpAicParameters = 0,
            int testFrameworkDump = 0);

    SkyCamProxy();
    virtual ~SkyCamProxy();

    virtual status_t init(int cameraId, IPU3ISPPipe **pipe, unsigned int numPipes, const ia_cmc_t* cmcParsed,
            const ia_binary_data* aiqb, IPU3AICRuntimeParams *runtimeParams,
            unsigned int dumpAicParameters = 0, int testFrameworkDump = 0) = 0;
    virtual void Run(IPU3AICRuntimeParams &runtime_params) = 0;
    virtual void Reset(IPU3AICRuntimeParams &runtime_params) = 0;
    virtual std::string GetAICVersion() = 0;
    virtual aic_config* GetAicConfig() = 0;

};

} /* namespace camera2 */
} /* namespace android */

#endif /* PSL_IPU3_SKYCAMPROXY_H_ */
