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

#define LOG_TAG "IA_FACE_ENGINE_IPC"

#include "LogHelper.h"
#include "IntelFaceEngine.h"
#include <utils/Errors.h>
#include "UtilityMacros.h"

namespace cros {
namespace intel {
IntelFaceEngine::IntelFaceEngine():
    mInitialized(false)
{
    LOG1("@%s", __FUNCTION__);

    mMems = {
        {"/faceEngineInitShm", sizeof(face_engine_init_params), &mMemInit, false},
        {"/faceEngineRunShm", sizeof(face_engine_run_params), &mMemRun, false}};

    bool success = mCommon.allocateAllShmMems(mMems);
    if (!success) {
        mCommon.releaseAllShmMems(mMems);
        return;
    }

    LOG1("@%s, done", __FUNCTION__);
    mInitialized = true;
}

IntelFaceEngine::~IntelFaceEngine()
{
    LOG1("@%s", __FUNCTION__);
    mCommon.releaseAllShmMems(mMems);
}

bool IntelFaceEngine::init(unsigned int max_face_num, int maxWidth, int maxHeight, face_detection_mode fd_mode)
{
    LOG1("@%s, max_face_num:%d, fd_mode:%d", __FUNCTION__, max_face_num, fd_mode);
    CheckError(mInitialized == false, false, "@%s, mInitialized is false", __FUNCTION__);

    if (maxWidth * maxHeight * 3 / 2 > MAX_FACE_FRAME_SIZE) {
        LOGE("@%s, maxWidth:%d, maxHeight:%d > MAX_FACE_FRAME_SIZE", __FUNCTION__, maxWidth, maxHeight);
        return false;
    }

    face_engine_init_params* params = static_cast<face_engine_init_params*>(mMemInit.mAddr);

    bool ret = mIpc.clientFlattenInit(max_face_num, fd_mode, params);
    CheckError(ret == false, false, "@%s, clientFlattenInit fails", __FUNCTION__);

    ret = mCommon.requestSync(IPC_FACE_INIT, mMemInit.mHandle);
    CheckError(ret == false, false, "@%s, requestSync fails", __FUNCTION__);

    return true;
}

void IntelFaceEngine::uninit()
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mInitialized == false, VOID_VALUE, "@%s, mInitialized is false", __FUNCTION__);

    bool ret = mCommon.requestSync(IPC_FACE_UNINIT);
    CheckError(ret == false, VOID_VALUE, "@%s, requestSync fails", __FUNCTION__);
}

bool IntelFaceEngine::prepareRun(const pvl_image& frame)
{
    LOG1("@%s, size:%d, w:%d, h:%d, f:%d, s:%d, r:%d", __FUNCTION__,
         frame.size, frame.width, frame.height, frame.format, frame.stride, frame.rotation);
    CheckError(mInitialized == false, false, "@%s, mInitialized is false", __FUNCTION__);

    face_engine_run_params* params = static_cast<face_engine_run_params*>(mMemRun.mAddr);
    bool ret = mIpc.clientFlattenRun(frame, params);
    CheckError(ret == false, false, "@%s, clientFlattenInit fails", __FUNCTION__);

    return true;
}

bool IntelFaceEngine::run(FaceEngineResult* results)
{
    LOG1("@%s", __FUNCTION__);
    CheckError(mInitialized == false, false, "@%s, mInitialized is false", __FUNCTION__);

    face_engine_run_params* params = static_cast<face_engine_run_params*>(mMemRun.mAddr);

    bool ret = mCommon.requestSync(IPC_FACE_RUN, mMemRun.mHandle);
    CheckError(ret == false, false, "@%s, requestSync fails", __FUNCTION__);

    *results = params->results;

    return true;
}

} /* namespace intel */
} /* namespace cros */
