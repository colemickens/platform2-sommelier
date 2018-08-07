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

#ifndef PSL_IPU3_IPC_SERVER_AICLIBRARY_H_
#define PSL_IPU3_IPC_SERVER_AICLIBRARY_H_

#include <utils/Errors.h>
#include <KBL_AIC.h>
#include "IPU3ISPPipe.h"
#include "IPCAic.h"

#include <memory>

namespace intel {
namespace camera {

class AicLibrary {
public:
    AicLibrary();
    virtual ~AicLibrary();

    status_t init(void* pData, int dataSize);
    void run(void* pData, int dataSize);
    void reset(void* pData, int dataSize);
    status_t getAicVersion(void* pData, int dataSize);
    status_t getAicConfig(void* pData, int dataSize);

private:
    std::unique_ptr<IPU3ISPPipe> mIspPipes[AIC_MODE_MAX][NUM_ISP_PIPES];
    std::unique_ptr<KBL_AIC> mSkyCam[AIC_MODE_MAX];

    IPCAic mIpc;
};

} /* namespace camera */
} /* namespace intel */
#endif // PSL_IPU3_IPC_SERVER_AICLIBRARY_H_
