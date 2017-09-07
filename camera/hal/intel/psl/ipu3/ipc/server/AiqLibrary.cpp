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

#define LOG_TAG "AiqLibrary"

#include <string.h>

#include "AiqLibrary.h"

#include <ia_aiq.h>
#include <ia_cmc_parser.h>

#include "IPCCommon.h"
#include "common/UtilityMacros.h"
#include "LogHelper.h"

namespace intel {
namespace camera {
AiqLibrary::AiqLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

AiqLibrary::~AiqLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

status_t AiqLibrary::aiq_init(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(aiq_init_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    aiq_init_params* params = static_cast<aiq_init_params*>(pData);

    ia_binary_data aiqbData = {nullptr, 0};
    ia_binary_data nvmData = {nullptr, 0};
    ia_binary_data aiqdData = {nullptr, 0};
    bool ret = mIpc.serverUnflattenInit(pData, dataSize, &aiqbData, &nvmData, &aiqdData);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflattenInit fails", __FUNCTION__);

    ia_aiq * aiq = ia_aiq_init(&aiqbData,
                                &nvmData,
                                &aiqdData,
                                params->stats_max_width,
                                params->stats_max_height,
                                params->max_num_stats_in,
                                reinterpret_cast<ia_cmc_t*>(params->cmcRemoteHandle),
                                reinterpret_cast<ia_mkn*>(params->ia_mkn));
    CheckError((aiq == nullptr), UNKNOWN_ERROR, "@%s, ia_aiq_init failed", __FUNCTION__);

    params->results = reinterpret_cast<uintptr_t>(aiq);

    return OK;
}

status_t AiqLibrary::aiq_deinit(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(aiq_deinit_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    aiq_deinit_params* params = static_cast<aiq_deinit_params*>(pData);
    ia_aiq_deinit(reinterpret_cast<ia_aiq*>(params->aiq_handle));

    return OK;
}

status_t AiqLibrary::aiq_af_run(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(af_run_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    af_run_params* params = static_cast<af_run_params*>(pData);

    ia_aiq_af_input_params* afParams = nullptr;
    bool ret = mIpc.serverUnflattenAf(*params, &afParams);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflatten fails", __FUNCTION__);

    ia_aiq_af_results* afResults = nullptr;
    ia_err err = ia_aiq_af_run(reinterpret_cast<ia_aiq*>(params->aiq_handle), afParams, &afResults);
    CheckError((err != 0), UNKNOWN_ERROR, "@%s, ia_aiq_af_run failed %d", __FUNCTION__, err);

    ret = mIpc.serverFlattenAf(*afResults, params);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverFlatten fails", __FUNCTION__);

    return OK;
}

status_t AiqLibrary::aiq_gbce_run(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(gbce_run_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    gbce_run_params *params = static_cast<gbce_run_params*>(pData);

    ia_aiq_gbce_results* gbceResults = nullptr;
    ia_err err = ia_aiq_gbce_run(reinterpret_cast<ia_aiq*>(params->aiq_handle), &params->base, &gbceResults);
    CheckError((err != 0), UNKNOWN_ERROR, "@%s, ia_aiq_gbce_run failed %d", __FUNCTION__, err);

    bool ret = mIpc.serverFlattenGbce(*gbceResults, params);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverFlattenGbce fails", __FUNCTION__);

    return OK;
}

status_t AiqLibrary::statistics_set(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(set_statistics_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    set_statistics_params* params = static_cast<set_statistics_params*>(pData);

    ia_aiq_statistics_input_params* stat = nullptr;
    bool ret = mIpc.serverUnflattenStat(*params, &stat);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflattenStat fails", __FUNCTION__);

    ia_err err = ia_aiq_statistics_set((ia_aiq*)params->ia_aiq, stat);
    CheckError((err != 0), UNKNOWN_ERROR, "@%s, ia_aiq_statistics_set failed %d", __FUNCTION__, err);

    return OK;
}

status_t AiqLibrary::aiq_ae_run(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(ae_run_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    ae_run_params* params = static_cast<ae_run_params*>(pData);

    ia_aiq_ae_input_params* aeParams = nullptr;
    bool ret = mIpc.serverUnflattenAe(*params, &aeParams);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflattenAe fails", __FUNCTION__);

    ia_aiq_ae_results *aeResults = nullptr;
    ia_err err = ia_aiq_ae_run(reinterpret_cast<ia_aiq*>(params->aiq_handle), aeParams, &aeResults);
    CheckError((err != 0), UNKNOWN_ERROR, "@%s, ia_aiq_ae_run failed %d", __FUNCTION__, err);

    ret = mIpc.serverFlattenAe(*aeResults, params);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverFlattenAe fails", __FUNCTION__);

    return OK;
}

status_t AiqLibrary::aiq_awb_run(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(awb_run_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    awb_run_params* params = static_cast<awb_run_params*>(pData);

    ia_aiq_awb_input_params* awbParams = nullptr;
    bool ret = mIpc.serverUnflattenAwb(*params, &awbParams);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflattenAwb fails", __FUNCTION__);

    ia_aiq_awb_results *awbResults = nullptr;
    ia_err err = ia_aiq_awb_run(reinterpret_cast<ia_aiq*>(params->aiq_handle), awbParams, &awbResults);
    CheckError((err != 0), UNKNOWN_ERROR, "@%s, ia_aiq_awb_run failed %d", __FUNCTION__, err);

    ret = mIpc.serverFlattenAwb(*awbResults, params);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverFlattenAwb fails", __FUNCTION__);

    return OK;
}

status_t AiqLibrary::aiq_pa_run(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(pa_run_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    pa_run_params* params = static_cast<pa_run_params*>(pData);

    ia_aiq_pa_input_params* paParams = nullptr;
    bool ret = mIpc.serverUnflattenPa(*params, &paParams);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflattenPa fails", __FUNCTION__);

    ia_aiq_pa_results* paResults = nullptr;
    ia_err err = ia_aiq_pa_run(reinterpret_cast<ia_aiq*>(params->aiq_handle), paParams, &paResults);
    CheckError((err != 0), UNKNOWN_ERROR, "@%s, ia_aiq_pa_run failed %d", __FUNCTION__, err);

    ret = mIpc.serverFlattenPa(*paResults, params);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverFlattenPa fails", __FUNCTION__);

    return OK;
}

status_t AiqLibrary::aiq_sa_run(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(sa_run_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    sa_run_params* params = static_cast<sa_run_params*>(pData);

    ia_aiq_sa_input_params* saParams = nullptr;
    bool ret = mIpc.serverUnflattenSa(*params, &saParams);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflattenSa fails", __FUNCTION__);

    ia_aiq_sa_results* saResults = nullptr;
    ia_err err = ia_aiq_sa_run(reinterpret_cast<ia_aiq*>(params->aiq_handle), saParams, &saResults);
    CheckError((err != 0), UNKNOWN_ERROR, "@%s, ia_aiq_sa_run failed %d", __FUNCTION__, err);

    ret = mIpc.serverFlattenSa(*saResults, params);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverFlattenSa fails", __FUNCTION__);

    return OK;
}

status_t AiqLibrary::aiq_get_aiqd_data(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(ia_binary_data_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    ia_binary_data binaryData = {nullptr, 0};

    ia_binary_data_params* params = static_cast<ia_binary_data_params*>(pData);

    ia_err err = ia_aiq_get_aiqd_data(reinterpret_cast<ia_aiq*>(params->aiq_handle), &binaryData);
    CheckError((err != 0), UNKNOWN_ERROR, "@%s, ia_aiq_get_aiqd_data failed %d", __FUNCTION__, err);
    LOG2("@%s, binary_data, data:%p, size:%d", __FUNCTION__, binaryData.data, binaryData.size);

    MEMCPY_S(params->data, sizeof(params->data), binaryData.data, binaryData.size);
    params->size = binaryData.size;

    return OK;
}

status_t AiqLibrary::aiq_get_version(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(ia_aiq_version_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    ia_aiq_version_params* params = static_cast<ia_aiq_version_params*>(pData);

    const char* version = ia_aiq_get_version();
    strncpy(params->data, version, sizeof(params->data));
    params->size = MIN(strlen(version), sizeof(params->data));
    LOG2("@%s, aiq version:%s, size:%d", __FUNCTION__, version, params->size);

    return OK;
}
} /* namespace camera */
} /* namespace intel */
