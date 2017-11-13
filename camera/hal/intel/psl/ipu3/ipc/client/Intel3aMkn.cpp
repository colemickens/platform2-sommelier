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

#define LOG_TAG "IA_MKN_IPC"

#include "LogHelper.h"
#include "Intel3aMkn.h"
#include <utils/Errors.h>
#include "UtilityMacros.h"

NAMESPACE_DECLARATION {
Intel3aMkn::Intel3aMkn():
    mInitialized(false)
{
    LOG1("@%s", __FUNCTION__);

    mMknHandle = reinterpret_cast<uintptr_t>(nullptr);

    mMems = {
        {"/mknInitShm", sizeof(mkn_init_params), &mMemInit, false},
        {"/mknUninitShm", sizeof(mkn_uninit_params), &mMemUninit, false},
        {"/mknPrepareShm", sizeof(mkn_prepare_params), &mMemPrepare, false},
        {"/mknEnableShm", sizeof(mkn_enable_params), &mMemEnable, false}};

    bool success = mCommon.allocateAllShmMems(mMems);
    if (!success) {
        mCommon.releaseAllShmMems(mMems);
        return;
    }

    LOG1("@%s, done", __FUNCTION__);
    mInitialized = true;
}

Intel3aMkn::~Intel3aMkn()
{
    LOG1("@%s", __FUNCTION__);
    mCommon.releaseAllShmMems(mMems);
}

bool Intel3aMkn::init(ia_mkn_config_bits mkn_config_bits,
            size_t mkn_section_1_size,
            size_t mkn_section_2_size)
{
    LOG1("@%s, mkn_config_bits:%d, mkn_section_1_size:%lu, mkn_section_2_size:%lu",
        __FUNCTION__, mkn_config_bits, mkn_section_1_size, mkn_section_2_size);
    CheckError(mInitialized == false, false, "@%s, mInitialized is false", __FUNCTION__);

    mkn_init_params* params = static_cast<mkn_init_params*>(mMemInit.mAddr);

    bool ret = mIpc.clientFlattenInit(mkn_config_bits,
                                mkn_section_1_size,
                                mkn_section_2_size,
                                params);
    CheckError(ret == false, false, "@%s, clientFlattenInit fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_MKN_INIT, mMemInit.mHandle);
    CheckError(ret == false, false, "@%s, requestSync fails", __FUNCTION__);

    mMknHandle = params->results;
    LOG2("@%s, mMknHandle:%p", __FUNCTION__, reinterpret_cast<ia_mkn*>(mMknHandle));

    return true;
}

void Intel3aMkn::uninit()
{
    LOG1("@%s", __FUNCTION__);
    CheckError(reinterpret_cast<ia_mkn*>(mMknHandle) == nullptr, VOID_VALUE, "@%s, mkn is nullptr", __FUNCTION__);
    CheckError(mInitialized == false, VOID_VALUE, "@%s, mInitialized is false", __FUNCTION__);

    mkn_uninit_params* params = static_cast<mkn_uninit_params*>(mMemUninit.mAddr);
    params->mkn_handle = mMknHandle;

    bool ret = mCommon.requestSync(IPC_3A_MKN_UNINIT, mMemUninit.mHandle);
    CheckError(ret == false, VOID_VALUE, "@%s, requestSync fails", __FUNCTION__);
}

ia_binary_data Intel3aMkn::prepare(ia_mkn_trg data_target)
{
    LOG1("@%s", __FUNCTION__);

    ia_binary_data mknData = {nullptr, 0};
    CheckError(reinterpret_cast<ia_mkn*>(mMknHandle) == nullptr, mknData, "@%s, mkn is nullptr", __FUNCTION__);
    CheckError(mInitialized == false, mknData, "@%s, mInitialized is false", __FUNCTION__);

    mkn_prepare_params* params = static_cast<mkn_prepare_params*>(mMemPrepare.mAddr);

    bool ret = mIpc.clientFlattenPrepare(mMknHandle, data_target, params);
    CheckError(ret == false, mknData, "@%s, clientFlattenPrepare fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_MKN_PREPARE, mMemPrepare.mHandle);
    CheckError(ret == false, mknData, "@%s, requestSync fails", __FUNCTION__);

    ret = mIpc.clientUnflattenPrepare(*params, &mknData);
    CheckError(ret == false, mknData, "@%s, clientUnflattenPrepare fails", __FUNCTION__);

    return mknData;
}

ia_err Intel3aMkn::enable(bool enable_data_collection)
{
    LOG1("@%s, enable_data_collection:%d", __FUNCTION__, enable_data_collection);

    CheckError(reinterpret_cast<ia_mkn*>(mMknHandle) == nullptr, ia_err_general, "@%s, mkn is nullptr", __FUNCTION__);
    CheckError(mInitialized == false, ia_err_general, "@%s, mInitialized is false", __FUNCTION__);

    mkn_enable_params* params = static_cast<mkn_enable_params*>(mMemEnable.mAddr);
    params->mkn_handle = mMknHandle;
    params->enable_data_collection = enable_data_collection;

    bool ret = mCommon.requestSync(IPC_3A_MKN_ENABLE, mMemEnable.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    return ia_err_none;
}

uintptr_t Intel3aMkn::getMknHandle() const
{
    LOG1("@%s", __FUNCTION__);

    return mMknHandle;
}
} NAMESPACE_DECLARATION_END
