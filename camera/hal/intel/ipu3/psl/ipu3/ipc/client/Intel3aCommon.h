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

#ifndef PSL_IPU3_IPC_CLIENT_INTEL3ACOMMON_H_
#define PSL_IPU3_IPC_CLIENT_INTEL3ACOMMON_H_

#include "Intel3AClient.h"

namespace cros {
namespace intel {
typedef struct ShmMemInfo {
    std::string mName;
    int mSize;
    int mFd;
    void* mAddr;
    int32_t mHandle;
    ShmMemInfo():
        mName(""),
        mSize(0),
        mFd(-1),
        mAddr(nullptr),
        mHandle(-1) {}
}ShmMemInfo;

typedef struct ShmMem {
    std::string name;
    int size;
    ShmMemInfo* mem;
    bool allocated;
} ShmMem;

class Intel3aCommon {
public:
    Intel3aCommon();
    virtual ~Intel3aCommon();

    bool allocShmMem(std::string& name, int size, ShmMemInfo* shm);
    bool requestSync(IPC_CMD cmd, int32_t handle);
    bool requestSync(IPC_CMD cmd);
    void freeShmMem(ShmMemInfo& shm);

    bool allocateAllShmMems(std::vector<ShmMem>& mems);
    void releaseAllShmMems(std::vector<ShmMem>& mems);

private:
    Intel3AClient* mClient;
};

} /* namespace intel */
} /* namespace cros */
#endif // PSL_IPU3_IPC_CLIENT_INTEL3ACOMMON_H_
