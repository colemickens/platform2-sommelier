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

#define LOG_TAG "SkyCamLocalProxy"

#include <KBL_AIC.h>
#include "SkyCamLocalProxy.h"
#include "LogHelper.h"

namespace android {
namespace camera2 {

SkyCamLocalProxy::SkyCamLocalProxy() :
        SkyCamProxy(),
        mSkyCam(nullptr),
        mPipe(nullptr)
{
}

SkyCamLocalProxy::~SkyCamLocalProxy()
{
    delete mSkyCam;
    mSkyCam = nullptr;
}

status_t SkyCamLocalProxy::init(int cameraId, IPU3ISPPipe **pipe, unsigned int numPipes, const ia_cmc_t* cmcParsed,
        const ia_binary_data* aiqb, IPU3AICRuntimeParams *runtimeParams,
        unsigned int dumpAicParameters, int testFrameworkDump)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    ISPPipe *tempISPPipes[NUM_ISP_PIPES];
    for (int i = 0; i < NUM_ISP_PIPES; i++) {
        tempISPPipes[i] = pipe[i];
    }
    mPipe = pipe[0];
    mSkyCam = new KBL_AIC(tempISPPipes, numPipes,
            cmcParsed,
            aiqb,
            *runtimeParams,
            dumpAicParameters,
            testFrameworkDump);

    return OK;
}

void SkyCamLocalProxy::Run(IPU3AICRuntimeParams &runtime_params)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mSkyCam->Run(&runtime_params, 1);
}

void SkyCamLocalProxy::Reset(IPU3AICRuntimeParams &runtime_params)
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    mSkyCam->Reset(runtime_params);
}

std::string SkyCamLocalProxy::GetAICVersion()
{
    HAL_TRACE_CALL(CAMERA_DEBUG_LOG_LEVEL1);
    return mSkyCam->GetAICVersion();
}

aic_config* SkyCamLocalProxy::GetAicConfig()
{
    return mPipe->GetAicConfig();
}

} /* namespace camera2 */
} /* namespace android */
