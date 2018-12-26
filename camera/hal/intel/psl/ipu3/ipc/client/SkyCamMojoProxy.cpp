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

#define LOG_TAG "SkyCamMojoProxy"

#include "UtilityMacros.h"
#include "SkyCamMojoProxy.h"

namespace android {
namespace camera2 {
SkyCamMojoProxy::SkyCamMojoProxy(AicMode aicMode):
        mInitialized(false),
        mAicMode(aicMode)
{
    LOG1("@%s, aicMode %d", __FUNCTION__, aicMode);

    std::string aicCommonShm = "/aicCommon" + std::to_string(aicMode) + "Shm";
    std::string aicCfgShm = "/aicCfg" + std::to_string(aicMode) + "Shm";
    std::string aicVersionShm = "/aicVersion" + std::to_string(aicMode) + "Shm";

    mMems = {
        {aicCommonShm.c_str(), sizeof(Transport), &mMemCommon, false},
        {aicCfgShm.c_str(), sizeof(IPU3AicConfig), &mMemCfg, false},
        {aicVersionShm.c_str(), sizeof(ia_aic_version_params), &mMemVersion, false}};

    bool success = mCommon.allocateAllShmMems(mMems);
    if (!success) {
        mCommon.releaseAllShmMems(mMems);
        return;
    }

    mInitialized = true;
}

SkyCamMojoProxy::~SkyCamMojoProxy()
{
    LOG1("@%s", __FUNCTION__);

    mCommon.releaseAllShmMems(mMems);
}

status_t SkyCamMojoProxy::init(int cameraId, IPU3ISPPipe** pipe, unsigned int numPipes,
        const ia_cmc_t* cmcParsed, const ia_binary_data* aiqb, IPU3AICRuntimeParams* runtimeParams,
        unsigned int dumpAicParameters, int testFrameworkDump)
{
    LOG1("@%s, cameraId:%d, pipe:%p, numPipes:%d, cmcParsed:%p, aiqb:%p, runtimeParams:%p, dumpAicParameters:%d, testFrameworkDump:%d",
        __FUNCTION__, cameraId, pipe, numPipes, cmcParsed, aiqb, runtimeParams, dumpAicParameters, testFrameworkDump);
    CheckError(mInitialized == false, UNKNOWN_ERROR, "@%s, mInitialized is false", __FUNCTION__);

    Transport* transport = static_cast<Transport*>(mMemCommon.mAddr);
    transport->aicMode = mAicMode;

    bool ret = mIpc.clientFlattenInit(*runtimeParams,
                                            numPipes,
                                            aiqb,
                                            reinterpret_cast<uintptr_t>(cmcParsed),
                                            dumpAicParameters,
                                            testFrameworkDump,
                                            transport);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, clientFlattenRun fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIC_INIT, mMemCommon.mHandle);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, requestSync fails", __FUNCTION__);

    return OK;
}

void SkyCamMojoProxy::Run(IPU3AICRuntimeParams& runtimeParams)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mInitialized == false, VOID_VALUE, "@%s, mInitialized is false", __FUNCTION__);

    Transport* transport = static_cast<Transport*>(mMemCommon.mAddr);
    transport->aicMode = mAicMode;

    bool ret = mIpc.clientFlattenRun(runtimeParams, transport);
    CheckError(ret == false, VOID_VALUE, "@%s, clientFlattenRun fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIC_RUN, mMemCommon.mHandle);
    CheckError(ret == false, VOID_VALUE, "@%s, requestSync fails", __FUNCTION__);
}

void SkyCamMojoProxy::Reset(IPU3AICRuntimeParams& runtimeParams)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mInitialized == false, VOID_VALUE, "@%s, mInitialized is false", __FUNCTION__);

    Transport* transport = static_cast<Transport*>(mMemCommon.mAddr);
    transport->aicMode = mAicMode;

    bool ret = mIpc.clientFlattenRun(runtimeParams, transport);
    CheckError(ret == false, VOID_VALUE, "@%s, clientFlattenRun fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIC_RESET, mMemCommon.mHandle);
    CheckError(ret == false, VOID_VALUE, "@%s, requestSync fails", __FUNCTION__);
}

std::string SkyCamMojoProxy::GetAICVersion()
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mInitialized == false, "", "@%s, mInitialized is false", __FUNCTION__);

    ia_aic_version_params* params = static_cast<ia_aic_version_params*>(mMemVersion.mAddr);
    params->aicMode = mAicMode;

    bool ret = mCommon.requestSync(IPC_3A_AIC_GETAICVERSION, mMemVersion.mHandle);
    CheckError(ret == false, "", "@%s, requestSync fails", __FUNCTION__);

    std::string version(params->data);
    LOG2("@%s, version:%s", __FUNCTION__, version.c_str());
    return version;
}

aic_config* SkyCamMojoProxy::GetAicConfig()
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mInitialized == false, nullptr, "@%s, mInitialized is false", __FUNCTION__);

    IPU3AicConfig* config = static_cast<IPU3AicConfig*>(mMemCfg.mAddr);
    config->aicMode = mAicMode;

    bool ret = mCommon.requestSync(IPC_3A_AIC_GETAICCONFIG, mMemCfg.mHandle);
    CheckError(ret == false, nullptr, "@%s, requestSync fails", __FUNCTION__);

    return &config->aicConfig;
}

} /* namespace camera2 */
} /* namespace android */
