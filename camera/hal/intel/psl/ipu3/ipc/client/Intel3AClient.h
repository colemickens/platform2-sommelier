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

#ifndef PSL_IPU3_IPC_CLIENT_INTEL3ACLIENT_H_
#define PSL_IPU3_IPC_CLIENT_INTEL3ACLIENT_H_
#include "ipc/IPCCommon.h"
#include <utils/Errors.h>
#include <mutex>
#include "cros-camera/camera_algorithm_bridge.h"
#include "base/bind.h"
#include "LogHelper.h"
#include <pthread.h>

#include "AAL/IErrorCallback.h"

namespace cros {
namespace intel {
class Intel3AClient : public camera_algorithm_callback_ops_t {
public:
    Intel3AClient();
    virtual ~Intel3AClient();

    bool isInitialized();
    bool isIPCFine();

    // when IPC error happens, device error
    // will be sent out via the IErrorCallback which belongs to ResultProcessor.
    // before the ResultProcessor be terminated, set nullptr in the function.
    void registerErrorCallback(IErrorCallback* errCb);

    int allocateShmMem(std::string& name, int size, int* fd, void** addr);
    void releaseShmMem(std::string& name, int size, int fd, void* addr);

    int requestSync(IPC_CMD cmd, int32_t bufferHandle);
    int requestSync(IPC_CMD cmd);

    int32_t registerBuffer(int bufferFd);
    void deregisterBuffer(int32_t bufferHandle);

private:
    int waitCallback();

    void callbackHandler(uint32_t req_id, uint32_t status, int32_t buffer_handle);
    void notifyHandler(uint32_t msg);

    // when the request is done, the callback will be received.
    static void returnCallback(const camera_algorithm_callback_ops_t* callback_ops,
                               uint32_t req_id,
                               uint32_t status,
                               int32_t buffer_handle);
    // when IPC error happens in the bridge, notifyCallback will be called.
    static void notifyCallback(const struct camera_algorithm_callback_ops* callback_ops,
                               camera_algorithm_error_msg_code_t msg);

private:
    IErrorCallback* mErrCb;

    std::unique_ptr<cros::CameraAlgorithmBridge> mBridge;

    base::Callback<void(uint32_t, uint32_t, int32_t)> mCallback;
    base::Callback<void(uint32_t)> mNotifyCallback;
    bool mIPCStatus; // true: no error happens, false: error happens
    std::mutex mIPCStatusMutex; // the mutex for mIPCStatus

    bool mInitialized;

private:
    class Runner {
    public:
        Runner(IPC_GROUP group, cros::CameraAlgorithmBridge* bridge);
        virtual ~Runner();
        int requestSync(IPC_CMD cmd, int32_t bufferHandle);
        void callbackHandler(uint32_t status, int32_t buffer_handle);

    private:
        int waitCallback();

    private:
        IPC_GROUP mGroup;
        cros::CameraAlgorithmBridge* mBridge;
        pthread_mutex_t mCbLock;
        pthread_cond_t mCbCond;
        bool mIsCallbacked;
        bool mCbResult; // true: success, false: fail

        bool mInitialized;

        std::mutex mMutex; // the mutex for the public method
    };

    std::unique_ptr<Runner> mRunner[IPC_GROUP_NUM];
};

} /* namespace intel */
} /* namespace cros */
#endif // PSL_IPU3_IPC_CLIENT_INTEL3ACLIENT_H_
