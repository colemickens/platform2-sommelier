/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#define LOG_TAG "Coordinate"

#include "LogHelper.h"
#include <ia_coordinate.h>
#include "CoordinateLibrary.h"
#include "IPCCoordinate.h"

using namespace android::camera2;

namespace intel {
namespace camera {

CoordinateLibrary::CoordinateLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

CoordinateLibrary::~CoordinateLibrary()
{
    LOG1("@%s", __FUNCTION__);
}

status_t CoordinateLibrary::convert(void* pData, int dataSize)
{
    LOG1("@%s, pData:%p, dataSize:%d", __FUNCTION__, pData, dataSize);
    CheckError((pData == nullptr), UNKNOWN_ERROR, "@%s, pData is nullptr", __FUNCTION__);
    CheckError((dataSize < sizeof(convert_coordinates_params)), UNKNOWN_ERROR, "@%s, buffer is small", __FUNCTION__);

    convert_coordinates_params* params = static_cast<convert_coordinates_params*>(pData);
    params->results = ia_coordinate_convert(&params->src_system, &params->trg_system, params->src_coordinate);

    return OK;
}
} /* namespace camera */
} /* namespace intel */
