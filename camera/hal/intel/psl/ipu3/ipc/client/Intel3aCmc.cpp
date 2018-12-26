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

#define LOG_TAG "IA_CMC_IPC"

#include "LogHelper.h"
#include "Intel3aCmc.h"
#include <utils/Errors.h>
#include "UtilityMacros.h"
#include "PlatformData.h"

namespace android {
namespace camera2 {
Intel3aCmc::Intel3aCmc(int cameraId):
    mInitialized(false)
{
    LOG1("@%s, cameraId:%d", __FUNCTION__, cameraId);

    mCmc = nullptr;
    mCmcRemoteHandle = reinterpret_cast<uintptr_t>(nullptr);

    CheckError((cameraId >= MAX_CAMERAS), VOID_VALUE,
        "@%s, cameraId:%d >= MAX_CAMERAS:%d", __FUNCTION__, cameraId, MAX_CAMERAS);

    std::string initName = "/cmcInit" + std::to_string(cameraId) + "Shm";
    std::string deinitName = "/cmcDeinit" + std::to_string(cameraId) + "Shm";

    mMems = {
        {initName.c_str(), sizeof(cmc_init_params), &mMemInit, false},
        {deinitName.c_str(), sizeof(cmc_deinit_params), &mMemDeinit, false}};

    bool success = mCommon.allocateAllShmMems(mMems);
    if (!success) {
        mCommon.releaseAllShmMems(mMems);
        return;
    }

    LOG1("@%s, done", __FUNCTION__);
    mInitialized = true;
}

Intel3aCmc::~Intel3aCmc()
{
    LOG1("@%s", __FUNCTION__);
    mCommon.releaseAllShmMems(mMems);
}

bool Intel3aCmc::init(const ia_binary_data* aiqb_binary)
{
    LOG1("@%s, aiqb_binary:%p", __FUNCTION__, aiqb_binary);

    CheckError(mInitialized == false, false, "@%s, mInitialized is false", __FUNCTION__);
    CheckError((aiqb_binary == nullptr), false, "@%s, aiqb_binary is nullptr", __FUNCTION__);
    CheckError((aiqb_binary->data == nullptr), false, "@%s, aiqb_binary->data is nullptr", __FUNCTION__);
    CheckError((aiqb_binary->size == 0), false, "@%s, aiqb_binary->size is 0", __FUNCTION__);

    cmc_init_params* params = static_cast<cmc_init_params*>(mMemInit.mAddr);

    bool ret = mIpc.clientFlattenInit(*aiqb_binary, params);
    CheckError(ret == false, false, "@%s, clientFlattenInit fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_CMC_INIT, mMemInit.mHandle);
    CheckError(ret == false, false, "@%s, requestSync fails", __FUNCTION__);

    ret = mIpc.clientUnflattenInit(*params, &mCmc, &mCmcRemoteHandle);
    CheckError(ret == false, false, "@%s, clientUnflattenInit fails", __FUNCTION__);

    return true;
}

void Intel3aCmc::deinit()
{
    LOG1("@%s, mCmc:%p", __FUNCTION__, mCmc);

    CheckError(mInitialized == false, VOID_VALUE, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(mCmc == nullptr, VOID_VALUE, "@%s, mCmc is nullptr", __FUNCTION__);
    CheckError(reinterpret_cast<ia_cmc_t*>(mCmcRemoteHandle) == nullptr,
        VOID_VALUE, "@%s, mCmcRemoteHandle is nullptr", __FUNCTION__);

    cmc_deinit_params* params = static_cast<cmc_deinit_params*>(mMemDeinit.mAddr);
    params->cmc_handle = mCmcRemoteHandle;

    bool ret = mCommon.requestSync(IPC_3A_CMC_DEINIT, mMemDeinit.mHandle);
    CheckError(ret == false, VOID_VALUE, "@%s, requestSync fails", __FUNCTION__);
    mCmc = nullptr;
    mCmcRemoteHandle = reinterpret_cast<uintptr_t>(nullptr);
}

ia_cmc_t* Intel3aCmc::getCmc() const
{
    LOG1("@%s, mCmc:%p", __FUNCTION__, mCmc);

    return mCmc;
}

uintptr_t Intel3aCmc::getCmcHandle() const
{
    LOG1("@%s", __FUNCTION__);

    return mCmcRemoteHandle;
}

} /* namespace camera2 */
} /* namespace android */
