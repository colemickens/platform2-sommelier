/*
 * Copyright (C) 2018-2019 Intel Corporation.
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

#ifndef PSL_IPU3_IPC_SERVER_INTEL3ASERVER_H_
#define PSL_IPU3_IPC_SERVER_INTEL3ASERVER_H_

#include <sys/un.h>

#include "IPU3ISPPipe.h"
#include "IPCCommon.h"
#include "AicLibrary.h"
#include "AiqLibrary.h"
#include "CmcLibrary.h"
#include "MknLibrary.h"
#include "CoordinateLibrary.h"
#include "ExcLibrary.h"

#include <base/bind.h>
#include <base/threading/thread.h>
#include "cros-camera/camera_algorithm.h"

#include <unordered_map>

namespace intel {
namespace camera {

class Intel3AServer {
public:
    static void init();
    static void deInit();

    static Intel3AServer* getInstance() { return mInstance; }

    int32_t initialize(const camera_algorithm_callback_ops_t* callback_ops);
    int32_t registerBuffer(int buffer_fd);
    void request(uint32_t req_id,
                 const uint8_t req_header[],
                 uint32_t size,
                 int32_t buffer_handle);
    void deregisterBuffers(const int32_t buffer_handles[], uint32_t size);

private:
    Intel3AServer();
    ~Intel3AServer();

    int parseReqHeader(const uint8_t req_header[], uint32_t size);

    struct MsgReq {
        uint32_t req_id;
        int32_t buffer_handle;
    };
    void returnCallback(uint32_t req_id, status_t status, int32_t buffer_handle);
    void handleRequest(MsgReq msg);

private:
    static Intel3AServer* mInstance;

    std::unique_ptr<base::Thread> mThreads[IPC_GROUP_NUM];
    const camera_algorithm_callback_ops_t* mCallback;

    bool mIaLogInitialized;

    // key: shared memory fd from client
    // value: handle that returns from RegisterBuffer()
    std::unordered_map<int32_t, int32_t> mHandles;

    typedef struct {
        int32_t fd;
        void* addr;
        size_t size;
    } ShmInfo;
    // key: handle that returns from RegisterBuffer()
    // value: shared memory fd and mapped address
    std::unordered_map<int32_t, ShmInfo> mShmInfoMap;

    AicLibrary mAic;
    CmcLibrary mCmc;
    ExcLibrary mExc;
    AiqLibrary mAiq;
    MknLibrary mMkn;
    CoordinateLibrary mCoordinate;

    int32_t mHandleSeed;
};

} /* namespace camera */
} /* namespace intel */
#endif // PSL_IPU3_IPC_SERVER_INTEL3ASERVER_H_
