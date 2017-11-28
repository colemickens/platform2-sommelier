/*
 * Copyright (C) 2017 Intel Corporation
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

#ifndef PSL_RKISP1_IPC_SERVER_AIQLIBRARY_H_
#define PSL_RKISP1_IPC_SERVER_AIQLIBRARY_H_

#include <utils/Errors.h>
#include "IPCCommon.h"
#include "IPCAiq.h"

namespace rockchip {
namespace camera {
class AiqLibrary {
public:
    AiqLibrary();
    virtual ~AiqLibrary();

    status_t aiq_init(void* pData, int dataSize);
    status_t aiq_deinit(void* pData, int dataSize);
    status_t aiq_misc_run(void* pData, int dataSize);
    status_t statistics_set(void* pData, int dataSize);
    status_t aiq_ae_run(void* pData, int dataSize);
    status_t aiq_awb_run(void* pData, int dataSize);
    status_t aiq_get_version(void* pData, int dataSize);

private:
    IPCAiq mIpc;
};

} /* namespace camera */
} /* namespace rockchip */

#endif // PSL_RKISP1_IPC_SERVER_AIQLIBRARY_H_
