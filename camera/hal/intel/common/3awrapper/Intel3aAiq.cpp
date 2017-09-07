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

#define LOG_TAG "Intel3aAiq"

#include "LogHelper.h"
#include "Intel3aAiq.h"

NAMESPACE_DECLARATION {
Intel3aAiq::Intel3aAiq():
        mAiq(nullptr)
{
    LOG1("@%s", __FUNCTION__);
}

Intel3aAiq::~Intel3aAiq()
{
    LOG1("@%s", __FUNCTION__);
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
    LOG1("@%s", __FUNCTION__);

    ia_cmc_t* cmc = reinterpret_cast<ia_cmc_t*>(cmc_handle);
    ia_mkn* mkn = reinterpret_cast<ia_mkn*>(mkn_handle);

    mAiq = ia_aiq_init(aiqb_data, nvm_data, aiqd_data, stats_max_width,
                        stats_max_height, max_num_stats_in, cmc, mkn);
    CheckError(mAiq == nullptr, false, "@%s, ia_aiq_init fails", __FUNCTION__);

    return true;
}

void Intel3aAiq::deinit()
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mAiq == nullptr, VOID_VALUE, "@%s, mAiq is nullptr", __FUNCTION__);

    ia_aiq_deinit(mAiq);
    mAiq = nullptr;
}

ia_err Intel3aAiq::aeRun(const ia_aiq_ae_input_params* ae_input_params,
              ia_aiq_ae_results** ae_results)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mAiq == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    return ia_aiq_ae_run(mAiq, ae_input_params, ae_results);
}

ia_err Intel3aAiq::afRun(const ia_aiq_af_input_params* af_input_params,
              ia_aiq_af_results** af_results)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mAiq == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    return ia_aiq_af_run(mAiq, af_input_params, af_results);
}

ia_err Intel3aAiq::awbRun(const ia_aiq_awb_input_params* awb_input_params,
               ia_aiq_awb_results** awb_results)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mAiq == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    return ia_aiq_awb_run(mAiq, awb_input_params, awb_results);
}

ia_err Intel3aAiq::gbceRun(const ia_aiq_gbce_input_params* gbce_input_params,
                ia_aiq_gbce_results** gbce_results)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mAiq == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    return ia_aiq_gbce_run(mAiq, gbce_input_params, gbce_results);
}

ia_err Intel3aAiq::getAiqdData(ia_binary_data* out_ia_aiq_data)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mAiq == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    return ia_aiq_get_aiqd_data(mAiq, out_ia_aiq_data);
}

ia_err Intel3aAiq::paRun(const ia_aiq_pa_input_params* pa_input_params,
               ia_aiq_pa_results** pa_results)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mAiq == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    return ia_aiq_pa_run(mAiq, pa_input_params, pa_results);
}

ia_err Intel3aAiq::saRun(const ia_aiq_sa_input_params* sa_input_params,
               ia_aiq_sa_results** sa_results)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mAiq == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    return ia_aiq_sa_run(mAiq, sa_input_params, sa_results);
}

ia_err Intel3aAiq::statisticsSet(const ia_aiq_statistics_input_params* input_params)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mAiq == nullptr, ia_err_general, "@%s, mAiq is nullptr", __FUNCTION__);

    return ia_aiq_statistics_set(mAiq, input_params);
}

const char* Intel3aAiq::getVersion(void)
{
    LOG1("@%s", __FUNCTION__);

    return ia_aiq_get_version();
}

bool Intel3aAiq::isInitialized() const
{
    LOG1("@%s", __FUNCTION__);

    return mAiq ? true : false;
}
} NAMESPACE_DECLARATION_END
