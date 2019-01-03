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

#define LOG_TAG "IPC_MKN"

#include <utils/Errors.h>

#include "common/UtilityMacros.h"
#include "LogHelper.h"
#include "IPCMkn.h"

namespace cros {
namespace intel {
IPCMkn::IPCMkn()
{
    LOG1("@%s", __FUNCTION__);
}

IPCMkn::~IPCMkn()
{
    LOG1("@%s", __FUNCTION__);
}

bool IPCMkn::clientFlattenInit(ia_mkn_config_bits mkn_config_bits,
                               size_t mkn_section_1_size,
                               size_t mkn_section_2_size,
                               mkn_init_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    params->mkn_config_bits = mkn_config_bits;
    params->mkn_section_1_size = mkn_section_1_size;
    params->mkn_section_2_size = mkn_section_2_size;

    return true;
}

bool IPCMkn::clientFlattenPrepare(uintptr_t mkn, ia_mkn_trg data_target, mkn_prepare_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    params->mkn_handle = mkn;
    params->data_target = data_target;

    return true;
}

bool IPCMkn::clientUnflattenPrepare(const mkn_prepare_params& params,
                               ia_binary_data* mknData)
{
    LOG1("@%s, mknData:%p", __FUNCTION__, mknData);
    CheckError(mknData == nullptr, false, "@%s, mknData is nullptr", __FUNCTION__);

    ia_binary_data_mod* results = const_cast<ia_binary_data_mod*>(&params.results);
    mknData->data = results->data;
    mknData->size = results->size;

    LOG2("@%s, mknData.size:%d", __FUNCTION__, mknData->size);

    return true;
}

bool IPCMkn::serverFlattenPrepare(const ia_binary_data& inData,
                               mkn_prepare_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError((params == nullptr), false, "@%s, params is nullptr", __FUNCTION__);

    ia_binary_data_mod* results = &params->results;

    MEMCPY_S(results->data, sizeof(results->data), inData.data, inData.size);
    results->size = inData.size;

    return true;
}

} /* namespace intel */
} /* namespace cros */
