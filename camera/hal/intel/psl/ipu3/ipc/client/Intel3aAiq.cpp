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

#define LOG_TAG "IA_AIQ_IPC"

#include <ia_aiq.h>
#include <ia_types.h>
#include <utils/Errors.h>

#include "Intel3aAiq.h"
#include "common/UtilityMacros.h"
#include "LogHelper.h"

namespace cros {
namespace intel {
Intel3aAiq::Intel3aAiq():
        mInitialized(false)
{
    LOG1("@%s", __FUNCTION__);

    mAiq = reinterpret_cast<uintptr_t>(nullptr);

    mMems = {
        {"/aiqDeinitShm", sizeof(aiq_deinit_params), &mMemDeinit, false},
        {"/aiqAeShm", sizeof(ae_run_params), &mMemAe, false},
        {"/aiqAfShm", sizeof(af_run_params), &mMemAf, false},
        {"/aiqAwbShm", sizeof(awb_run_params), &mMemAwb, false},
        {"/aiqGbceShm", sizeof(gbce_run_params), &mMemGbce, false},
        {"/aiqAiqdShm", sizeof(ia_binary_data_params), &mMemAiqd, false},
        {"/aiqPaShm", sizeof(pa_run_params), &mMemPa, false},
        {"/aiqSaShm", sizeof(sa_run_params), &mMemSa, false},
        {"/aiqStatShm", sizeof(set_statistics_params), &mMemStat, false},
        {"/aiqVersionShm", sizeof(ia_aiq_version_params), &mMemVersion, false}};

    bool success = mCommon.allocateAllShmMems(mMems);
    if (!success) {
        mCommon.releaseAllShmMems(mMems);
        return;
    }

    LOG1("@%s, done", __FUNCTION__);
    mInitialized = true;
}

Intel3aAiq::~Intel3aAiq()
{
    LOG1("@%s", __FUNCTION__);
    mCommon.releaseAllShmMems(mMems);
}

bool Intel3aAiq::init(const ia_binary_data* aiqb_data,
            const ia_binary_data* nvm_data,
            const ia_binary_data* aiqd_data,
            unsigned int stats_max_width,
            unsigned int stats_max_height,
            unsigned int max_num_stats_in,
            uintptr_t cmc_handle,
            uintptr_t mkn_handle)
{
    LOG1("@%s, aiqb_data:%p, nvm_data:%p, aiqd_data:%p", __FUNCTION__, aiqb_data, nvm_data, aiqd_data);

    CheckError(mInitialized == false, false, "@%s, mInitialized is false", __FUNCTION__);

    if (aiqb_data)
        LOG2("aiqb_data->size:%d", aiqb_data->size);
    if (nvm_data)
        LOG2("nvm_data->size:%d", nvm_data->size);
    if (aiqd_data)
        LOG2("aiqd_data->size:%d", aiqd_data->size);

    unsigned int aiqbSize = aiqb_data ? aiqb_data->size : 0;
    unsigned int nvmSize = nvm_data ? nvm_data->size : 0;
    unsigned int aiqdSize = aiqd_data ? aiqd_data->size : 0;
    unsigned int size = sizeof(aiq_init_params) + aiqbSize + nvmSize + aiqdSize;

    ShmMemInfo shm;
    shm.mName = "/aiqInitShm";
    shm.mSize = size;
    bool ret = mCommon.allocShmMem(shm.mName, shm.mSize, &shm);
    CheckError(ret == false, false, "@%s, allocShmMem fails", __FUNCTION__);

    ret = mIpc.clientFlattenInit(aiqb_data, aiqbSize,
                                nvm_data, nvmSize,
                                aiqd_data, aiqdSize,
                                stats_max_width,
                                stats_max_height,
                                max_num_stats_in,
                                mkn_handle,
                                cmc_handle,
                                (uint8_t*)shm.mAddr, size);
    CheckError(ret == false, false, "@%s, clientFlattenInit fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIQ_INIT, shm.mHandle);
    CheckError(ret == false, false, "@%s, requestSync fails", __FUNCTION__);

    aiq_init_params* params = static_cast<aiq_init_params*>(shm.mAddr);
    mAiq = params->results;
    LOG2("@%s, success, aiq:%p\n", __FUNCTION__, reinterpret_cast<ia_aiq*>(mAiq));

    mCommon.freeShmMem(shm);

    return true;
}

void Intel3aAiq::deinit()
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mInitialized == false, VOID_VALUE, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(reinterpret_cast<ia_aiq*>(mAiq) == nullptr, VOID_VALUE, "@%s, mAiq is nullptr", __FUNCTION__);

    aiq_deinit_params* params = static_cast<aiq_deinit_params*>(mMemDeinit.mAddr);
    params->aiq_handle = mAiq;

    int ret = mCommon.requestSync(IPC_3A_AIQ_DEINIT, mMemDeinit.mHandle);
    CheckError(ret == false, VOID_VALUE, "@%s, requestSync fails", __FUNCTION__);

    mAiq = reinterpret_cast<uintptr_t>(nullptr);
}

ia_err Intel3aAiq::aeRun(const ia_aiq_ae_input_params* ae_input_params,
              ia_aiq_ae_results** ae_results)
{
    LOG1("@%s, ae_input_params:%p, ae_results:%p", __FUNCTION__, ae_input_params, ae_results);
    CheckError(mInitialized == false, ia_err_general, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(reinterpret_cast<ia_aiq*>(mAiq) == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    CheckError((ae_input_params == nullptr), ia_err_argument, "@%s, ae_input_params is nullptr", __FUNCTION__);
    CheckError((ae_results == nullptr), ia_err_argument, "@%s, ae_results is nullptr", __FUNCTION__);

    ae_run_params* params = static_cast<ae_run_params*>(mMemAe.mAddr);

    bool ret = mIpc.clientFlattenAe(mAiq, *ae_input_params, params);
    CheckError(ret == false, ia_err_general, "@%s, clientFlattenAe fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIQ_AE_RUN, mMemAe.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    ret = mIpc.clientUnflattenAe(*params, ae_results);
    CheckError(ret == false, ia_err_general, "@%s, clientUnflattenAe fails", __FUNCTION__);

    return ia_err_none;
}

ia_err Intel3aAiq::afRun(const ia_aiq_af_input_params* af_input_params,
              ia_aiq_af_results** af_results)
{
    LOG1("@%s, af_input_params:%p, af_results:%p", __FUNCTION__, af_input_params, af_results);
    CheckError(mInitialized == false, ia_err_general, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(reinterpret_cast<ia_aiq*>(mAiq) == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    CheckError((af_input_params == nullptr), ia_err_argument, "@%s, af_input_params is nullptr", __FUNCTION__);
    CheckError((af_results == nullptr), ia_err_argument, "@%s, af_results is nullptr", __FUNCTION__);

    af_run_params* params = static_cast<af_run_params*>(mMemAf.mAddr);

    bool ret = mIpc.clientFlattenAf(mAiq, *af_input_params, params);
    CheckError(ret == false, ia_err_general, "@%s, clientFlattenAf fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIQ_AF_RUN, mMemAf.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    ret = mIpc.clientUnflattenAf(*params, af_results);
    CheckError(ret == false, ia_err_general, "@%s, clientUnflattenAf fails", __FUNCTION__);

    return ia_err_none;
}

ia_err Intel3aAiq::awbRun(const ia_aiq_awb_input_params* awb_input_params,
               ia_aiq_awb_results** awb_results)
{
    LOG1("@%s, awb_input_params:%p, awb_results:%p", __FUNCTION__, awb_input_params, awb_results);
    CheckError(mInitialized == false, ia_err_general, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(reinterpret_cast<ia_aiq*>(mAiq) == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    CheckError((awb_input_params == nullptr), ia_err_argument, "@%s, awb_input_params is nullptr", __FUNCTION__);
    CheckError((awb_results == nullptr), ia_err_argument, "@%s, awb_results is nullptr", __FUNCTION__);

    awb_run_params* params = static_cast<awb_run_params*>(mMemAwb.mAddr);

    bool ret = mIpc.clientFlattenAwb(mAiq, *awb_input_params, params);
    CheckError(ret == false, ia_err_general, "@%s, clientFlattenAwb fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIQ_AWB_RUN, mMemAwb.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    ret = mIpc.clientUnflattenAwb(*params, awb_results);
    CheckError(ret == false, ia_err_general, "@%s, clientUnflattenAwb fails", __FUNCTION__);

    return ia_err_none;
}

ia_err Intel3aAiq::gbceRun(const ia_aiq_gbce_input_params* gbce_input_params,
                ia_aiq_gbce_results** gbce_results)
{
    LOG1("@%s, gbce_input_params:%p, gbce_results:%p", __FUNCTION__, gbce_input_params, gbce_results);
    CheckError(mInitialized == false, ia_err_general, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(reinterpret_cast<ia_aiq*>(mAiq) == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    CheckError((gbce_input_params == nullptr), ia_err_argument, "@%s, gbce_input_params is nullptr", __FUNCTION__);
    CheckError((gbce_results == nullptr), ia_err_argument, "@%s, gbce_results is nullptr", __FUNCTION__);

    gbce_run_params* params = static_cast<gbce_run_params*>(mMemGbce.mAddr);

    bool ret = mIpc.clientFlattenGbce(mAiq, *gbce_input_params, params);
    CheckError(ret == false, ia_err_general, "@%s, clientFlattenGbce fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIQ_GBCE_RUN, mMemGbce.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    ret = mIpc.clientUnflattenGbce(*params, gbce_results);
    CheckError(ret == false, ia_err_general, "@%s, clientUnflattenGbce fails", __FUNCTION__);

    return ia_err_none;
}

ia_err Intel3aAiq::getAiqdData(ia_binary_data* out_ia_aiq_data)
{
    LOG1("@%s, out_ia_aiq_data:%p", __FUNCTION__, out_ia_aiq_data);
    CheckError(mInitialized == false, ia_err_general, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(reinterpret_cast<ia_aiq*>(mAiq) == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    CheckError((out_ia_aiq_data == nullptr), ia_err_argument, "@%s, out_ia_aiq_data is nullptr", __FUNCTION__);

    ia_binary_data_params* params = static_cast<ia_binary_data_params*>(mMemAiqd.mAddr);

    params->aiq_handle = mAiq;

    int ret = mCommon.requestSync(IPC_3A_AIQ_GET_AIQ_DATA, mMemAiqd.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    out_ia_aiq_data->data = params->data;
    out_ia_aiq_data->size = params->size;

    return ia_err_none;
}

ia_err Intel3aAiq::paRun(const ia_aiq_pa_input_params* pa_input_params,
               ia_aiq_pa_results** pa_results)
{
    LOG1("@%s, pa_input_params:%p, pa_results:%p", __FUNCTION__, pa_input_params, pa_results);
    CheckError(mInitialized == false, ia_err_general, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(reinterpret_cast<ia_aiq*>(mAiq) == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    CheckError((pa_input_params == nullptr), ia_err_argument, "@%s, pa_input_params is nullptr", __FUNCTION__);
    CheckError((pa_results == nullptr), ia_err_argument, "@%s, pa_results is nullptr", __FUNCTION__);

    pa_run_params* params = static_cast<pa_run_params*>(mMemPa.mAddr);

    bool ret = mIpc.clientFlattenPa(mAiq, *pa_input_params, params);
    CheckError(ret == false, ia_err_general, "@%s, clientFlattenPa fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIQ_PA_RUN, mMemPa.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    ret = mIpc.clientUnflattenPa(*params, pa_results);
    CheckError(ret == false, ia_err_general, "@%s, clientUnflattenPa fails", __FUNCTION__);

    return ia_err_none;
}

ia_err Intel3aAiq::saRun(const ia_aiq_sa_input_params* sa_input_params,
               ia_aiq_sa_results** sa_results)
{
    LOG1("@%s, sa_input_params:%p, sa_results:%p", __FUNCTION__, sa_input_params, sa_results);
    CheckError(mInitialized == false, ia_err_general, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(reinterpret_cast<ia_aiq*>(mAiq) == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    CheckError((sa_input_params == nullptr), ia_err_argument, "@%s, sa_input_params is nullptr", __FUNCTION__);
    CheckError((sa_results == nullptr), ia_err_argument, "@%s, sa_results is nullptr", __FUNCTION__);

    sa_run_params* params = static_cast<sa_run_params*>(mMemSa.mAddr);

    bool ret = mIpc.clientFlattenSa(mAiq, *sa_input_params, params);
    CheckError(ret == false, ia_err_general, "@%s, clientFlattenSa fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIQ_SA_RUN, mMemSa.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    ret = mIpc.clientUnflattenSa(*params, sa_results);
    CheckError(ret == false, ia_err_general, "@%s, clientUnflattenSa fails", __FUNCTION__);

    return ia_err_none;
}

ia_err Intel3aAiq::statisticsSet(const ia_aiq_statistics_input_params* input_params)
{
    LOG1("@%s, input_params:%p", __FUNCTION__, input_params);
    CheckError(mInitialized == false, ia_err_general, "@%s, mInitialized is false", __FUNCTION__);
    CheckError(reinterpret_cast<ia_aiq*>(mAiq) == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    CheckError((input_params == nullptr), ia_err_argument, "@%s, input_params is nullptr", __FUNCTION__);

    set_statistics_params* params = static_cast<set_statistics_params*>(mMemStat.mAddr);

    bool ret = mIpc.clientFlattenStat(mAiq, *input_params, params);
    CheckError(ret == false, ia_err_general, "@%s, clientFlattenStat fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_3A_AIQ_STATISTICS_SET, mMemStat.mHandle);
    CheckError(ret == false, ia_err_general, "@%s, requestSync fails", __FUNCTION__);

    return ia_err_none;
}

const char* Intel3aAiq::getVersion(void)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mInitialized == false, "null", "@%s, mInitialized is false", __FUNCTION__);

    int ret = mCommon.requestSync(IPC_3A_AIQ_GET_VERSION, mMemVersion.mHandle);
    CheckError(ret == false, "null", "@%s, requestSync fails", __FUNCTION__);

    ia_aiq_version_params* params = static_cast<ia_aiq_version_params*>(mMemVersion.mAddr);
    return params->data;
}

bool Intel3aAiq::isInitialized() const
{
    LOG1("@%s", __FUNCTION__);

    return mAiq ? true : false;
}
} /* namespace intel */
} /* namespace cros */
