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

#define LOG_TAG "Intel3AClient"
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <sys/mman.h>

#include "Intel3AClient.h"
#include "Utils.h"

NAMESPACE_DECLARATION {
Intel3AClient::Intel3AClient():
    mIsCallbacked(false),
    mCbResult(true),
    mInitialized(false)
{
    LOG1("@%s", __FUNCTION__);

    int ret = pthread_cond_init(&mCbCond, NULL);
    CheckError(ret != 0, VOID_VALUE, "@%s, call pthread_cond_init fails, ret:%d", __FUNCTION__, ret);

    ret = pthread_mutex_init(&mCbLock, NULL);
    CheckError(ret != 0, VOID_VALUE, "@%s, call pthread_mutex_init fails, ret:%d", __FUNCTION__, ret);

    mCallback = base::Bind(&Intel3AClient::callbackHandler, base::Unretained(this));
    Intel3AClient::return_callback = returnCallback;

    mBridge = arc::CameraAlgorithmBridge::GetInstance();
    CheckError((mBridge->Initialize(this) != 0), VOID_VALUE, "@%s, call mBridge->Initialize fail", __FUNCTION__);

    mInitialized = true;
}

Intel3AClient::~Intel3AClient()
{
    LOG1("@%s", __FUNCTION__);

    int ret = pthread_cond_destroy(&mCbCond);
    if (ret != 0) {
        LOGE("@%s, call pthread_cond_destroy fails, ret:%d", __FUNCTION__, ret);
    }

    ret = pthread_mutex_destroy(&mCbLock);
    if (ret != 0) {
        LOGE("@%s, call pthread_mutex_destroy fails, ret:%d", __FUNCTION__, ret);
    }
}

Intel3AClient* Intel3AClient::getInstance()
{
    LOG1("@%s", __FUNCTION__);
    static Intel3AClient client;
    return client.mInitialized ? &client: nullptr;
}

int Intel3AClient::allocateShmMem(std::string& name, int size, int* fd, void** addr)
{
    LOG1("@%s, name:%s, size:%d, mInitialized:%d", __FUNCTION__, name.c_str(), size, mInitialized);
    CheckError((mInitialized != true), UNKNOWN_ERROR, "@%s, mInitialized is false", __FUNCTION__);

    *fd = -1;
    *addr = nullptr;
    int shmFd = -1;
    void* shmAddr = nullptr;

    shmFd = shm_open(name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    CheckError((shmFd == -1), UNKNOWN_ERROR, "@%s, call shm_open fail", __FUNCTION__);

    int ret = fcntl(shmFd, F_GETFD);
    CheckError((ret == -1), UNKNOWN_ERROR, "@%s, call fcntl fail", __FUNCTION__);

    ret = ftruncate(shmFd, size);
    CheckError((ret == -1), UNKNOWN_ERROR, "@%s, call fcntl fail", __FUNCTION__);

    struct stat sb;
    ret = fstat(shmFd, &sb);
    CheckError((ret == -1), UNKNOWN_ERROR, "@%s, call fstat fail", __FUNCTION__);
    CheckError((sb.st_size != size), UNKNOWN_ERROR, "@%s, sb.st_size:%d", __FUNCTION__, sb.st_size);

    shmAddr = mmap(0, sb.st_size, PROT_WRITE, MAP_SHARED, shmFd, 0);
    CheckError((!shmAddr), UNKNOWN_ERROR, "@%s, call mmap fail", __FUNCTION__);

    *fd = shmFd;
    *addr = shmAddr;

    return OK;
}

void Intel3AClient::releaseShmMem(std::string& name, int size, int fd, void* addr)
{
    LOG1("@%s, name:%s, size:%d, fd:%d, addr:%p", __FUNCTION__, name.c_str(), size, fd, addr);
    CheckError((mInitialized != true), VOID_VALUE, "@%s, mInitialized is false", __FUNCTION__);

    munmap(addr, size);
    close(fd);
    shm_unlink(name.c_str());
}

int Intel3AClient::waitCallback()
{
    LOG2("@%s", __FUNCTION__);
    nsecs_t startTime = systemTime();

    struct timeval tv;
    struct timespec ts;
    gettimeofday(&tv, nullptr);
    ts.tv_sec = tv.tv_sec + 1; // 1s timeout
    ts.tv_nsec = tv.tv_usec * 1000L;

    pthread_mutex_lock(&mCbLock);
    if (!mIsCallbacked) {
        int ret = pthread_cond_timedwait(&mCbCond, &mCbLock, &ts);
        CheckError(ret != 0, UNKNOWN_ERROR, "@%s, call pthread_cond_timedwait fail, ret:%d", __FUNCTION__, ret);
    }
    mIsCallbacked = false;
    pthread_mutex_unlock(&mCbLock);

    LOG2("@%s: it takes %ums", __FUNCTION__, (unsigned)((systemTime() - startTime) / 1000000));

    return OK;
}

int Intel3AClient::requestSync(IPC_CMD cmd, int32_t bufferHandle)
{
    LOG1("@%s, cmd:%d:%s, bufferHandle:%d, mInitialized:%d",
        __FUNCTION__, cmd, Intel3AIpcCmdToString(cmd), bufferHandle, mInitialized);
    CheckError((mInitialized != true), UNKNOWN_ERROR, "@%s, mInitialized is false", __FUNCTION__);

    std::lock_guard<std::mutex> lck(mMutex);

    std::vector<uint8_t> reqHeader(IPC_REQUEST_HEADER_USED_NUM);
    reqHeader[0] = IPC_MATCHING_KEY;
    reqHeader[1] = cmd & 0xff;

    mBridge->Request(reqHeader, bufferHandle);
    int ret = waitCallback();
    CheckError((ret != OK), UNKNOWN_ERROR, "@%s, call waitCallback fail", __FUNCTION__);

    LOG2("@%s, cmd:%d:%s, mCbResult:%d, done!",
        __FUNCTION__, cmd, Intel3AIpcCmdToString(cmd), mCbResult);

    // check callback result
    CheckError((mCbResult != true), UNKNOWN_ERROR, "@%s, callback fail", __FUNCTION__);

    return OK;
}

int Intel3AClient::requestSync(IPC_CMD cmd)
{
    LOG1("@%s, cmd:%d:%s, mInitialized:%d",
        __FUNCTION__, cmd, Intel3AIpcCmdToString(cmd), mInitialized);
    return requestSync(cmd, -1);
}

int32_t Intel3AClient::RegisterBuffer(int bufferFd)
{
    LOG1("@%s, bufferFd:%d, mInitialized:%d", __FUNCTION__, bufferFd, mInitialized);
    CheckError((mInitialized != true), -1, "@%s, mInitialized is false", __FUNCTION__);

    return mBridge->RegisterBuffer(bufferFd);
}

void Intel3AClient::DeregisterBuffer(int32_t bufferHandle)
{
    LOG1("@%s, bufferHandle:%d, mInitialized:%d", __FUNCTION__, bufferHandle, mInitialized);
    CheckError((mInitialized != true), VOID_VALUE, "@%s, mInitialized is false", __FUNCTION__);

    std::vector<int32_t> handles({bufferHandle});
    mBridge->DeregisterBuffers(handles);
}

void Intel3AClient::callbackHandler(uint32_t status, int32_t buffer_handle)
{
    LOG2("@%s, status:%d, buffer_handle:%d", __FUNCTION__, status, buffer_handle);
    if (status != 0) {
        LOGE("@%s, status:%d, buffer_handle:%d", __FUNCTION__, status, buffer_handle);
    }
    mCbResult = status != 0 ? false : true;

    pthread_mutex_lock(&mCbLock);
    mIsCallbacked = true;
    int ret = pthread_cond_signal(&mCbCond);
    pthread_mutex_unlock(&mCbLock);

    CheckError(ret != 0, VOID_VALUE, "@%s, call pthread_cond_signal fails, ret:%d", __FUNCTION__, ret);
}

void Intel3AClient::returnCallback(const camera_algorithm_callback_ops_t* callback_ops,
                            uint32_t status, int32_t buffer_handle) {
    LOG2("@%s", __FUNCTION__);
    CheckError(!callback_ops, VOID_VALUE, "@%s, callback_ops is nullptr", __FUNCTION__);

    auto s = const_cast<Intel3AClient*>(static_cast<const Intel3AClient*>(callback_ops));
    s->mCallback.Run(status, buffer_handle);
}
} NAMESPACE_DECLARATION_END
