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

#define LOG_TAG "Intel3aCoordinate"

#include "LogHelper.h"
#include "Intel3aCoordinate.h"
#include "IPCCoordinate.h"

namespace android {
namespace camera2 {
Intel3aCoordinate::Intel3aCoordinate():
    mInitialized(false)
{
    LOG1("@%s", __FUNCTION__);

    mMem.mName = "/coordinateShm";
    mMem.mSize = sizeof(convert_coordinates_params);
    bool ret = mCommon.allocShmMem(mMem.mName, mMem.mSize, &mMem);
    CheckError(ret == false, VOID_VALUE, "@%s, %s allocShmMem fails", __FUNCTION__, mMem.mName.c_str());

    LOG1("@%s, done", __FUNCTION__);
    mInitialized = true;
}

Intel3aCoordinate::~Intel3aCoordinate()
{
    LOG1("@%s", __FUNCTION__);
    mCommon.freeShmMem(mMem);
}

ia_coordinate
Intel3aCoordinate::convert(
                    const ia_coordinate_system& src_system,
                    const ia_coordinate_system& trg_system,
                    const ia_coordinate& src_coordinate)
{
    LOG1("@%s, src_system:%p, trg_system:%p", __FUNCTION__, &src_system, &trg_system);
    LOG1("@%s, src_coordinate.x:%d, src_coordinate.y:%d",
        __FUNCTION__, src_coordinate.x, src_coordinate.y);

    ia_coordinate re = {-1, -1};
    CheckError(mInitialized == false, re, "@%s, mInitialized is false", __FUNCTION__);

    convert_coordinates_params* params = static_cast<convert_coordinates_params*>(mMem.mAddr);

    params->src_system = src_system;
    params->trg_system = trg_system;
    params->src_coordinate = src_coordinate;

    bool ret = mCommon.requestSync(IPC_3A_COORDINATE_COVERT, mMem.mHandle);
    CheckError(ret == false, re, "@%s, requestSync fails", __FUNCTION__);

    return params->results;
}

} /* namespace camera2 */
} /* namespace android */
