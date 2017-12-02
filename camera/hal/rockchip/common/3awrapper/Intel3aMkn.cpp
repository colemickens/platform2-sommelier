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

#define LOG_TAG "Intel3aMkn"

#include "LogHelper.h"
#include "Intel3aMkn.h"

NAMESPACE_DECLARATION {

Intel3aMkn::Intel3aMkn()
{
    LOG1("@%s", __FUNCTION__);

    mMknHandle = reinterpret_cast<uintptr_t>(nullptr);
}

Intel3aMkn::~Intel3aMkn()
{
    LOG1("@%s", __FUNCTION__);
}

bool Intel3aMkn::init(ia_mkn_config_bits mkn_config_bits,
              size_t mkn_section_1_size,
              size_t mkn_section_2_size)
{
    LOG1("@%s", __FUNCTION__);
    ia_mkn* mkn = ia_mkn_init(mkn_config_bits,
              mkn_section_1_size,
              mkn_section_2_size);
    CheckError(mkn == nullptr, false, "@%s, ia_mkn_init fails", __FUNCTION__);

    mMknHandle = reinterpret_cast<uintptr_t>(mkn);

    return true;
}

void Intel3aMkn::uninit()
{
    LOG1("@%s", __FUNCTION__);

    ia_mkn* mkn = reinterpret_cast<ia_mkn*>(mMknHandle);
    CheckError(mkn == nullptr, VOID_VALUE, "@%s, mkn is nullptr", __FUNCTION__);
    ia_mkn_uninit(mkn);

    mMknHandle = reinterpret_cast<uintptr_t>(nullptr);
}

ia_binary_data Intel3aMkn::prepare(ia_mkn_trg data_target)
{
    LOG1("@%s", __FUNCTION__);
    ia_binary_data ret = {nullptr, 0};

    ia_mkn* mkn = reinterpret_cast<ia_mkn*>(mMknHandle);
    CheckError(mkn == nullptr, ret, "@%s, mkn is nullptr", __FUNCTION__);
    return ia_mkn_prepare(mkn, data_target);
}

ia_err Intel3aMkn::enable(bool enable_data_collection)
{
    LOG1("@%s", __FUNCTION__);

    ia_mkn* mkn = reinterpret_cast<ia_mkn*>(mMknHandle);
    CheckError(mkn == nullptr, ia_err_general, "@%s, mkn is nullptr", __FUNCTION__);
    return ia_mkn_enable(mkn, enable_data_collection);
}

uintptr_t Intel3aMkn::getMknHandle() const
{
    LOG1("@%s", __FUNCTION__);

    return mMknHandle;
}
} NAMESPACE_DECLARATION_END
