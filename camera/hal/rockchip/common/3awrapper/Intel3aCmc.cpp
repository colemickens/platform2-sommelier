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

#define LOG_TAG "Intel3aCmc"

#include "LogHelper.h"
#include "Intel3aCmc.h"

NAMESPACE_DECLARATION {
Intel3aCmc::Intel3aCmc(int cameraId):
        mCmc(nullptr)
{
    LOG1("@%s, cameraId:%d", __FUNCTION__, cameraId);
}

Intel3aCmc::~Intel3aCmc()
{
    LOG1("@%s", __FUNCTION__);
}

bool Intel3aCmc::init(const ia_binary_data* aiqb_binary)
{
    LOG1("@%s", __FUNCTION__);

    mCmc = ia_cmc_parser_init(aiqb_binary);
    CheckError(mCmc == nullptr, false, "@%s, ia_cmc_parser_init fails", __FUNCTION__);

    return true;
}

void Intel3aCmc::deinit()
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mCmc == nullptr, VOID_VALUE, "@%s, mCmc is nullptr", __FUNCTION__);

    ia_cmc_parser_deinit(mCmc);
    mCmc = nullptr;
}

ia_cmc_t* Intel3aCmc::getCmc() const
{
    LOG1("@%s", __FUNCTION__);

    return mCmc;
}

uintptr_t Intel3aCmc::getCmcHandle() const
{
    LOG1("@%s", __FUNCTION__);

    return reinterpret_cast<uintptr_t>(mCmc);
}
} NAMESPACE_DECLARATION_END
