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

#define LOG_TAG "SkyCamProxy"

#include <ia_cmc_types.h>
#include <ia_types.h>
#include <cpffData.h>
#include <Pipe.h>
#include <KBL_AIC.h>

#include "LogHelper.h"
#include "SkyCamProxy.h"
#include "ipc/client/SkyCamMojoProxy.h"

namespace android {
namespace camera2 {

SkyCamProxy::SkyCamProxy()
{
    // TODO Auto-generated constructor stub
}

SkyCamProxy::~SkyCamProxy()
{
    // TODO Auto-generated destructor stub
}

std::shared_ptr<SkyCamProxy> SkyCamProxy::createProxy(int cameraId, IPU3ISPPipe **pipes, unsigned int numPipes, const ia_cmc_t* cmcParsed,
        const ia_binary_data* aiqb, IPU3AICRuntimeParams *runtimeParams,
        unsigned int dumpAicParameters, int testFrameworkDump)
{

    std::shared_ptr<SkyCamProxy> proxyObject = nullptr;

    LOGD("Use IPC implementation");
    proxyObject = std::make_shared<SkyCamMojoProxy>();

    status_t ret = proxyObject->init(cameraId, pipes, numPipes, cmcParsed, aiqb, runtimeParams,
                    dumpAicParameters, testFrameworkDump);

    if (ret != OK) {
        LOGE("Cannot initialize proxy AIC");
        return nullptr;
    }

    return proxyObject;
}

} /* namespace camera2 */
} /* namespace android */
