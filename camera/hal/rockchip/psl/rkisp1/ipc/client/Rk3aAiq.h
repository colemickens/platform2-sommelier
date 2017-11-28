/*
 * Copyright (C) 2017 Intel Corporation.
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef PSL_RKISP1_IPC_CLIENT_RK3AAIQ_H_
#define PSL_RKISP1_IPC_CLIENT_RK3AAIQ_H_
#include <vector>

#include <rk_aiq.h>
#include "Rk3aCommon.h"
#include "IPCAiq.h"

NAMESPACE_DECLARATION {
class Rk3aAiq {
public:
    Rk3aAiq();
    virtual ~Rk3aAiq();

    bool init(const char* xmlFilePath);
    void deinit();
    status_t aeRun(const rk_aiq_ae_input_params* ae_input_params,
                   rk_aiq_ae_results** ae_results);
    status_t awbRun(const rk_aiq_awb_input_params* awb_input_params,
                    rk_aiq_awb_results** awb_results);
    status_t miscRun(const rk_aiq_misc_isp_input_params* misc_input_params,
                     rk_aiq_misc_isp_results** misc_results);
    status_t statisticsSet(const rk_aiq_statistics_input_params* input_params,
                           const rk_aiq_exposure_sensor_descriptor *sensor_desc = nullptr);
    const char* getVersion(void);

    bool isInitialized() const;

private:
    IPCAiq mIpc;
    Rk3aCommon mCommon;

    uintptr_t mAiq;

    bool mInitialized;

    ShmMemInfo mMemInit;
    ShmMemInfo mMemDeinit;
    ShmMemInfo mMemAe;
    ShmMemInfo mMemAwb;
    ShmMemInfo mMemMisc;
    ShmMemInfo mMemStat;
    ShmMemInfo mMemVersion;

    std::vector<ShmMem> mMems;
};
} NAMESPACE_DECLARATION_END
#endif // PSL_RKISP1_IPC_CLIENT_RK3AAIQ_H_
