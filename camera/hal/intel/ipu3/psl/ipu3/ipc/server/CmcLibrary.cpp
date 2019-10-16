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

#define LOG_TAG "CmcLibrary"

#include <string.h>
#include "LogHelper.h"
#include <ia_cmc_parser.h>
#include "CmcLibrary.h"

namespace cros {
namespace intel {

CmcLibrary::CmcLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

CmcLibrary::~CmcLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

status_t CmcLibrary::ia_cmc_init(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(cmc_init_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    cmc_init_params* params = static_cast<cmc_init_params*>(pData);
    ia_binary_data aiqbData = {nullptr, 0};

    bool ret = mIpc.serverUnflattenInit(*params, &aiqbData);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverUnflattenInit fails", __FUNCTION__);

    ia_cmc_t* cmc = ia_cmc_parser_init(&aiqbData);
    CheckError((cmc == nullptr), UNKNOWN_ERROR, "@%s, ia_cmc_parser_init failed", __FUNCTION__);

    LOG1("@%s, cmc:%p", __FUNCTION__, cmc);

    ret = mIpc.serverFlattenInit(*cmc, params);
    CheckError(ret == false, UNKNOWN_ERROR, "@%s, serverFlattenInit fails", __FUNCTION__);

    return OK;
}

status_t CmcLibrary::ia_cmc_deinit(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(cmc_deinit_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    cmc_deinit_params* params = static_cast<cmc_deinit_params*>(pData);

    ia_cmc_t* cmc = reinterpret_cast<ia_cmc_t*>(params->cmc_handle);
    LOG1("@%s, cmc:%p", __FUNCTION__, cmc);

    ia_cmc_parser_deinit(cmc);

    return OK;
}
} /* namespace intel */
} /* namespace cros */
