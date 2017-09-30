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

#ifndef PSL_IPU3_IPC_CLIENT_INTEL3ACLIENT_H_
#define PSL_IPU3_IPC_CLIENT_INTEL3ACLIENT_H_
#include "ipc/IPCCommon.h"
#include <utils/Errors.h>
#include <mutex>
#include "arc/camera_algorithm_bridge.h"
#include "base/bind.h"
#include "LogHelper.h"
#include <pthread.h>

NAMESPACE_DECLARATION {
class Intel3AClient : public camera_algorithm_callback_ops_t {
public:
    static Intel3AClient* getInstance();

    static void release();

    int allocateShmMem(std::string& name, int size, int* fd, void** addr);
    void releaseShmMem(std::string& name, int size, int fd, void* addr);

    int requestSync(IPC_CMD cmd, int32_t bufferHandle);
    int requestSync(IPC_CMD cmd);

    int32_t RegisterBuffer(int bufferFd);
    void DeregisterBuffer(int32_t bufferHandle);

private:
    Intel3AClient();
    ~Intel3AClient();

    int waitCallback();

    void callbackHandler(uint32_t status, int32_t buffer_handle);

    static void returnCallback(const camera_algorithm_callback_ops_t* callback_ops,
                                uint32_t status, int32_t buffer_handle);

private:
    static Intel3AClient* mInstance;

    arc::CameraAlgorithmBridge* mBridge;
    pthread_mutex_t mCbLock;
    pthread_cond_t mCbCond;
    bool mIsCallbacked;

    base::Callback<void(uint32_t, int32_t)> mCallback;
    bool mCbResult; // true: success, false: fail

    bool mInitialized;

    std::mutex mMutex; // the mutex for the public method
};
} NAMESPACE_DECLARATION_END
#endif // PSL_IPU3_IPC_CLIENT_INTEL3ACLIENT_H_
