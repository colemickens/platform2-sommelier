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

#define LOG_TAG "Intel3AServer"

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "Intel3AServer.h"
#include "LogHelper.h"
#include "RuntimeParamsHelper.h"

namespace intel {
namespace camera {

Intel3AServer* Intel3AServer::getInstance()
{
    LOG1("@%s", __FUNCTION__);
    static Intel3AServer instance;
    return &instance;
}

int32_t Intel3AServer::initialize(const camera_algorithm_callback_ops_t* callback_ops)
{
    LOG1("@%s, callback_ops:%p", __FUNCTION__, callback_ops);

    CheckError((!callback_ops), -EINVAL, "@%s, the callback_ops is nullptr", __FUNCTION__);

    mCallback = callback_ops;
    return 0;
}

int32_t Intel3AServer::registerBuffer(int buffer_fd)
{
    LOG1("@%s, buffer_fd:%d", __FUNCTION__, buffer_fd);
    CheckError((mHandles.find(buffer_fd) != mHandles.end()),
                -EINVAL, "@%s, Buffer already registered", __FUNCTION__);

    struct stat sb;
    int ret = fstat(buffer_fd, &sb);
    CheckError((ret == -1), -EBADFD, "@%s, Failed to get buffer status", __FUNCTION__);

    void* addr = mmap(0, sb.st_size, PROT_WRITE, MAP_SHARED, buffer_fd, 0);
    CheckError((!addr), -EBADFD, "@%s, Failed to map buffer", __FUNCTION__);

    int32_t handle = mHandleSeed++;
    mHandles[buffer_fd] = handle;

    mShmInfoMap[handle].fd = buffer_fd;
    mShmInfoMap[handle].addr = addr;
    mShmInfoMap[handle].size = sb.st_size;

    return handle;
}

int Intel3AServer::parseReqHeader(const uint8_t req_header[], uint32_t size, uint8_t* cmd)
{
    LOG1("@%s, size:%d", __FUNCTION__, size);
    if (size < IPC_REQUEST_HEADER_USED_NUM || req_header[0] != IPC_MATCHING_KEY) {
        LOGE("@%s, fail, request header size:%d, req_header[0]:%d", __FUNCTION__, size, req_header[0]);
        return -1;
    }

    *cmd = req_header[1];

    LOG2("@%s, size:%d, cmd:%d:%s, requestSize:%d",
        __FUNCTION__, size, *cmd, Intel3AIpcCmdToString((IPC_CMD)(*cmd)));

    return 0;
}

uint32_t Intel3AServer::handleRequest(uint8_t cmd, int reqeustSize, void* addr)
{
    LOG1("@%s, cmd:%d:%s, reqeustSize:%d, addr:%p",
        __FUNCTION__, cmd, Intel3AIpcCmdToString((IPC_CMD)cmd), reqeustSize, addr);

    if (cmd != IPC_3A_AIC_RESET && cmd != IPC_3A_CMC_DEINIT) {
        CheckError((addr == nullptr), 1, "@%s, addr is nullptr", __FUNCTION__);
    }

    status_t status = OK;
    switch (cmd) {
        case IPC_3A_AIC_INIT:
            status = mAic.init(addr, reqeustSize);
            break;
        case IPC_3A_AIC_RUN:
            mAic.run(addr, reqeustSize);
            break;
        case IPC_3A_AIC_RESET:
            mAic.reset(addr, reqeustSize);
            break;
        case IPC_3A_AIC_GETAICVERSION:
            status = mAic.getAicVersion(addr, reqeustSize);
            break;
        case IPC_3A_AIC_GETAICCONFIG:
            status = mAic.getAicConfig(addr, reqeustSize);
            break;
        case IPC_3A_CMC_INIT:
            status = mCmc.ia_cmc_init(addr, reqeustSize);
            break;
        case IPC_3A_CMC_DEINIT:
            status = mCmc.ia_cmc_deinit(addr, reqeustSize);
            break;
        case IPC_3A_EXC_ANALOG_GAIN_TO_SENSOR:
            status = mExc.analog_gain_to_sensor_units(addr, reqeustSize);
            break;
        case IPC_3A_EXC_SENSOR_TO_ANALOG_GAIN:
            status = mExc.sensor_units_to_analog_gain(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_INIT:
            status = mAiq.aiq_init(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_DEINIT:
            status = mAiq.aiq_deinit(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_AE_RUN:
            status = mAiq.aiq_ae_run(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_AF_RUN:
            status = mAiq.aiq_af_run(addr, reqeustSize);;
            break;
        case IPC_3A_AIQ_AWB_RUN:
            status = mAiq.aiq_awb_run(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_PA_RUN:
            status = mAiq.aiq_pa_run(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_SA_RUN:
            status = mAiq.aiq_sa_run(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_GBCE_RUN:
            status = mAiq.aiq_gbce_run(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_GET_AIQ_DATA:
            status = mAiq.aiq_get_aiqd_data(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_STATISTICS_SET:
            status = mAiq.statistics_set(addr, reqeustSize);
            break;
        case IPC_3A_AIQ_GET_VERSION:
            status = mAiq.aiq_get_version(addr, reqeustSize);
            break;
        case IPC_3A_MKN_INIT:
            status = mMkn.init(addr, reqeustSize);
            break;
        case IPC_3A_MKN_UNINIT:
            status = mMkn.uninit(addr, reqeustSize);
            break;
        case IPC_3A_MKN_PREPARE:
            status = mMkn.prepare(addr, reqeustSize);
            break;
        case IPC_3A_MKN_ENABLE:
            status = mMkn.enable(addr, reqeustSize);
            break;
        case IPC_3A_COORDINATE_COVERT:
            status = mCoordinate.convert(addr, reqeustSize);
            break;
        default:
            LOGE("@%s, cmd:%d is not defined", __FUNCTION__, cmd);
            break;
    }

    LOG2("@%s, cmd:%d:%s, status:%d", __FUNCTION__, cmd, Intel3AIpcCmdToString((IPC_CMD)cmd), status);
    return status == OK ? 0 : 1;
}

void Intel3AServer::request(const uint8_t req_header[], uint32_t size, int32_t buffer_handle)
{
    LOG1("@%s, size:%d, buffer_handle:%d", __FUNCTION__, size, buffer_handle);
    uint32_t status = 0;

    uint8_t cmd = -1;
    int ret = parseReqHeader(req_header, size, &cmd);
    CheckError((ret != 0), VOID_VALUE, "@%s, call parseReqHeader fail", __FUNCTION__);

    LOG2("@%s, buffer_handle:%d", __FUNCTION__, buffer_handle);
    if (buffer_handle == -1) {
        status = handleRequest(cmd, 0, nullptr);
    } else {
        CheckError((mShmInfoMap.find(buffer_handle) == mShmInfoMap.end()),
                    VOID_VALUE, "@%s, Invalid buffer handle", __FUNCTION__);
        ShmInfo info = mShmInfoMap[buffer_handle];

        LOG2("@%s, info.fd:%d, info.size:%d", __FUNCTION__, info.fd, info.size);
        status = handleRequest(cmd, info.size, info.addr);
    }

    mThread.task_runner()->PostTask(FROM_HERE,
        base::Bind(&Intel3AServer::returnCallback, base::Unretained(this), status, buffer_handle));
}

void Intel3AServer::deregisterBuffers(const int32_t buffer_handles[], uint32_t size)
{
    LOG1("@%s, size:%d", __FUNCTION__, size);
    for (uint32_t i = 0; i < size; i++) {
        int32_t handle = buffer_handles[i];
        if (mShmInfoMap.find(handle) == mShmInfoMap.end()) {
            continue;
        }

        mHandles.erase(mShmInfoMap[handle].fd);

        munmap(mShmInfoMap[handle].addr, mShmInfoMap[handle].size);
        close(mShmInfoMap[handle].fd);
        mShmInfoMap.erase(handle);
    }
}

void Intel3AServer::returnCallback(uint32_t status, int32_t buffer_handle)
{
    LOG1("@%s, buffer_handle:%d", __FUNCTION__, buffer_handle);
    (*mCallback->return_callback)(mCallback, status, buffer_handle);
}


static int32_t initialize(const camera_algorithm_callback_ops_t* callback_ops)
{
    LOG1("@%s, callback_ops:%p", __FUNCTION__, callback_ops);
    return Intel3AServer::getInstance()->initialize(callback_ops);
}

static int32_t registerBuffer(int32_t buffer_fd)
{
    LOG1("@%s, buffer_fd:%d", __FUNCTION__, buffer_fd);
    return Intel3AServer::getInstance()->registerBuffer(buffer_fd);
}

static void request(const uint8_t req_header[],
                       uint32_t size,
                       int32_t buffer_handle)
{
    LOG1("@%s, size:%d, buffer_handle:%d", __FUNCTION__, size, buffer_handle);
    Intel3AServer::getInstance()->request(req_header, size, buffer_handle);
}

static void deregisterBuffers(const int32_t buffer_handles[], uint32_t size)
{
    LOG1("@%s, size:%d", __FUNCTION__, size);
    return Intel3AServer::getInstance()->deregisterBuffers(buffer_handles, size);
}

extern "C" {
camera_algorithm_ops_t CAMERA_ALGORITHM_MODULE_INFO_SYM
    __attribute__((__visibility__("default"))) = {
        .initialize = initialize,
        .register_buffer = registerBuffer,
        .request = request,
        .deregister_buffers = deregisterBuffers
    };
}

} /* namespace camera */
} /* namespace intel */
