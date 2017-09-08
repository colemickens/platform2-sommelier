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
#ifndef PSL_IPU3_IPC_CLIENT_INTEL3ACMC_H_
#define PSL_IPU3_IPC_CLIENT_INTEL3ACMC_H_
#include <vector>
#include <ia_cmc_parser.h>
#include "Intel3aCommon.h"
#include "IPCCmc.h"

NAMESPACE_DECLARATION {
class Intel3aCmc {
public:
    Intel3aCmc(int cameraId);
    virtual ~Intel3aCmc();

    bool init(const ia_binary_data* aiqb_binary);
    void deinit();

    ia_cmc_t* getCmc() const;
    uintptr_t getCmcHandle() const;

private:
    IPCCmc mIpc;
    Intel3aCommon mCommon;

    bool mInitialized;

    ShmMemInfo mMemDeinit;
    ShmMemInfo mMemInit;

    std::vector<ShmMem> mMems;

    ia_cmc_t* mCmc;

    // the pointer is in another process.
    // because the ia_aiq_init() needs the cmc pointer,
    // so keep the pinter in the Intel3aCmc and pass it aiq when init aiq.
    // at the same time, it will be used when deinit the cmc.
    uintptr_t mCmcRemoteHandle;
};
} NAMESPACE_DECLARATION_END
#endif //PSL_IPU3_IPC_CLIENT_INTEL3ACMC_H_
