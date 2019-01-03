/*
 * Copyright (C) 2018 Intel Corporation
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

#define LOG_TAG "IPC_FACE_ENGINE"

#include <utils/Errors.h>

#include "common/UtilityMacros.h"
#include "LogHelper.h"
#include "IPCFaceEngine.h"

namespace cros {
namespace intel {
IPCFaceEngine::IPCFaceEngine()
{
    LOG1("@%s", __FUNCTION__);
}

IPCFaceEngine::~IPCFaceEngine()
{
    LOG1("@%s", __FUNCTION__);
}

bool IPCFaceEngine::clientFlattenInit(unsigned int max_face_num,
                                      face_detection_mode fd_mode,
                                      face_engine_init_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    params->max_face_num = max_face_num;
    params->fd_mode = fd_mode;

    return true;
}

bool IPCFaceEngine::clientFlattenRun(const pvl_image& frame, face_engine_run_params* params)
{
    LOG1("@%s, params:%p", __FUNCTION__, params);
    CheckError(params == nullptr, false, "@%s, params is nullptr", __FUNCTION__);

    if (frame.size > MAX_FACE_FRAME_SIZE) {
        LOGE("@%s, face frame buffer is too small!, w:%d,h:%d,size:%d",
             __FUNCTION__, frame.width, frame.height, frame.size);
        return false;
    }

    params->size = frame.size;
    MEMCPY_S(params->data, MAX_FACE_FRAME_SIZE, frame.data, frame.size);
    params->width = frame.width;
    params->height = frame.height;
    params->format = frame.format;
    params->stride = frame.stride;
    params->rotation = frame.rotation;

    return true;
}

bool IPCFaceEngine::serverUnflattenRun(const face_engine_run_params& inParams, pvl_image* image)
{
    LOG1("@%s, image:%p", __FUNCTION__, image);
    CheckError(image == nullptr, false, "@%s, iamge is nullptr", __FUNCTION__);

    image->data = const_cast<uint8_t*>(inParams.data);
    image->size = inParams.size;
    image->width = inParams.width;
    image->height = inParams.height;
    image->format = inParams.format;
    image->stride = inParams.stride;
    image->rotation = inParams.rotation;

    return true;
}

} /* namespace intel */
} /* namespace cros */
