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

#ifndef PSL_IPU3_IPC_CLIENT_INTEL3AMKN_H_
#define PSL_IPU3_IPC_CLIENT_INTEL3AMKN_H_
#include <vector>
#include <ia_types.h>

#include "Intel3aCommon.h"
#include "IPCMkn.h"

namespace cros {
namespace intel {
class Intel3aMkn {
public:
    Intel3aMkn();
    virtual ~Intel3aMkn();

    bool init(ia_mkn_config_bits mkn_config_bits,
                  size_t mkn_section_1_size,
                  size_t mkn_section_2_size);

    void uninit();

    ia_binary_data prepare(ia_mkn_trg data_target);

    ia_err enable(bool enable_data_collection);

    uintptr_t getMknHandle() const;

private:
    IPCMkn mIpc;
    Intel3aCommon mCommon;

    bool mInitialized;

    ShmMemInfo mMemInit;
    ShmMemInfo mMemUninit;
    ShmMemInfo mMemPrepare;
    ShmMemInfo mMemEnable;

    std::vector<ShmMem> mMems;

    uintptr_t mMknHandle;
};
} /* namespace intel */
} /* namespace cros */
#endif //PSL_IPU3_IPC_CLIENT_INTEL3AMKN_H_
