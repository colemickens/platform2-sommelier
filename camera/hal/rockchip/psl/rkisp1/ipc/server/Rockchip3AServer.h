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

#ifndef PSL_RKISP1_IPC_SERVER_RK3ASERVER_H_
#define PSL_RKISP1_IPC_SERVER_RK3ASERVER_H_

#include <sys/un.h>

#include "IPCCommon.h"
#include "AiqLibrary.h"

#include <base/bind.h>
#include <base/threading/thread.h>
#include "arc/camera_algorithm.h"

#include <unordered_map>

namespace rockchip {
namespace camera {

class Rockchip3AServer {
public:
    static void init();
    static void deInit();

    static Rockchip3AServer* getInstance() { return mInstance; }

    int32_t initialize(const camera_algorithm_callback_ops_t* callback_ops);
    int32_t registerBuffer(int buffer_fd);
    void request(const uint8_t req_header[], uint32_t size, int32_t buffer_handle);
    void deregisterBuffers(const int32_t buffer_handles[], uint32_t size);

private:
    Rockchip3AServer();
    ~Rockchip3AServer();

    void returnCallback(uint32_t status, int32_t buffer_handle);
    int parseReqHeader(const uint8_t req_header[], uint32_t size, uint8_t* cmd);
    uint32_t handleRequest(uint8_t cmd, int reqeustSize, void* addr);

private:
    static Rockchip3AServer* mInstance;

    base::Thread mThread;
    const camera_algorithm_callback_ops_t* mCallback;

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

    AiqLibrary mAiq;

    int32_t mHandleSeed;
};

} /* namespace camera */
} /* namespace rockchip */
#endif // PSL_RKISP1_IPC_SERVER_RK3ASERVER_H_
