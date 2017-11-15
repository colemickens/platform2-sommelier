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

#define LOG_TAG "MknLibrary"

#include "MknLibrary.h"

#include <ia_mkn_encoder.h>
#include <string.h>

#include "LogHelper.h"
#include "common/UtilityMacros.h"

namespace intel {
namespace camera {
MknLibrary::MknLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

MknLibrary::~MknLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

status_t MknLibrary::init(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(mkn_init_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    mkn_init_params* params = static_cast<mkn_init_params*>(pData);

    ia_mkn* mkn = ia_mkn_init(params->mkn_config_bits, params->mkn_section_1_size, params->mkn_section_2_size);

    params->results = reinterpret_cast<uintptr_t>(mkn);
    LOG2("@%s, mkn:%p, params->results:%" PRIxPTR, __FUNCTION__, mkn, params->results);

    return OK;
}

status_t MknLibrary::uninit(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(mkn_uninit_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    mkn_uninit_params* params = static_cast<mkn_uninit_params*>(pData);
    LOG2("@%s, params->mkn_handle:%p", __FUNCTION__, reinterpret_cast<ia_mkn*>(params->mkn_handle));

    ia_mkn_uninit(reinterpret_cast<ia_mkn*>(params->mkn_handle));

    return OK;
}

status_t MknLibrary::prepare(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(mkn_prepare_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    mkn_prepare_params* params = static_cast<mkn_prepare_params*>(pData);

    ia_mkn* mkn = reinterpret_cast<ia_mkn*>(params->mkn_handle);
    ia_binary_data data = ia_mkn_prepare(mkn, params->data_target);
    LOG2("@%s, data.size:%d, data.data:%p", __FUNCTION__, data.size, data.data);

    if (!data.size
        || data.data == nullptr) {
        LOGE("@%s, data.size:%d, data.data:%p, error!",
            __FUNCTION__, data.size, data.data);
        return NO_MEMORY;
    }

    bool ret = mIpc.serverFlattenPrepare(data, params);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverFlattenPrepare fails", __FUNCTION__);

    return OK;
}

status_t MknLibrary::enable(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(mkn_enable_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    mkn_enable_params* params = static_cast<mkn_enable_params*>(pData);

    ia_mkn* mkn = reinterpret_cast<ia_mkn*>(params->mkn_handle);
    ia_err err = ia_mkn_enable(mkn, params->enable_data_collection);
    CheckError(err != ia_err_none, ia_err_general, "@%s, call ia_mkn_enable() fails", __FUNCTION__);

    return err == ia_err_none ? NO_ERROR : UNKNOWN_ERROR;
}

} /* namespace camera */
} /* namespace intel */
