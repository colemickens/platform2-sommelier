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

#ifndef PSL_IPU3_IPC_CLIENT_SKYCAMMOJOPROXY_H_
#define PSL_IPU3_IPC_CLIENT_SKYCAMMOJOPROXY_H_

#include <memory>
#include "SkyCamProxy.h"
#include "ipc/IPCCommon.h"
#include "Intel3aCommon.h"
#include "IPCAic.h"

NAMESPACE_DECLARATION {
class SkyCamMojoProxy: public SkyCamProxy {
public:
    SkyCamMojoProxy(AicMode aicMode);
    virtual ~SkyCamMojoProxy();
    status_t init(int cameraId, IPU3ISPPipe** pipe, unsigned int numPipes,
                  const ia_cmc_t* cmcParsed,
                  const ia_binary_data* aiqb,
                  IPU3AICRuntimeParams* runtimeParams,
                  unsigned int dumpAicParameters = 0,
                  int test_frameworkDump = 0);
    virtual void Run(IPU3AICRuntimeParams& runtimeParams);
    virtual void Reset(IPU3AICRuntimeParams& runtimeParams);
    virtual std::string GetAICVersion();
    virtual aic_config* GetAicConfig();

private:
    IPCAic mIpc;
    Intel3aCommon mCommon;

    bool mInitialized;
    AicMode mAicMode;

    ShmMemInfo mMemCommon;
    ShmMemInfo mMemCfg;
    ShmMemInfo mMemVersion;

    std::vector<ShmMem> mMems;
};
} NAMESPACE_DECLARATION_END
#endif // PSL_IPU3_IPC_CLIENT_SKYCAMMOJOPROXY_H_
