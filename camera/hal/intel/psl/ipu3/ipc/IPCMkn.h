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

#ifndef PSL_IPU3_IPC_IPCMKN_H_
#define PSL_IPU3_IPC_IPCMKN_H_
#include <ia_mkn_encoder.h>
#include "IPCCommon.h"

struct mkn_init_params {
    ia_mkn_config_bits mkn_config_bits;
    size_t mkn_section_1_size;
    size_t mkn_section_2_size;
    uintptr_t results;
};

struct mkn_prepare_params {
    uintptr_t mkn_handle;
    ia_mkn_trg data_target;
    ia_binary_data_mod results;
};

struct mkn_uninit_params {
    uintptr_t mkn_handle;
};

struct mkn_enable_params {
    uintptr_t mkn_handle;
    bool enable_data_collection;
};

namespace android {
namespace camera2 {
class IPCMkn {
public:
    IPCMkn();
    virtual ~IPCMkn();

    // for init
    bool clientFlattenInit(ia_mkn_config_bits mkn_config_bits,
                           size_t mkn_section_1_size,
                           size_t mkn_section_2_size,
                           mkn_init_params* params);

    // for prepare
    bool clientFlattenPrepare(uintptr_t mkn, ia_mkn_trg data_target,
                           mkn_prepare_params* params);
    bool clientUnflattenPrepare(const mkn_prepare_params& params,
                           ia_binary_data* mknData);
    bool serverFlattenPrepare(const ia_binary_data& inData,
                           mkn_prepare_params* results);
};
} /* namespace camera2 */
} /* namespace android */
#endif // PSL_IPU3_IPC_IPCMKN_H_
