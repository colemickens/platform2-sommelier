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

NAMESPACE_DECLARATION {
Intel3aCoordinate::Intel3aCoordinate()
{
    LOG1("@%s", __FUNCTION__);
}

Intel3aCoordinate::~Intel3aCoordinate()
{
    LOG1("@%s", __FUNCTION__);
}

ia_coordinate
Intel3aCoordinate::convert(
                    const ia_coordinate_system& src_system,
                    const ia_coordinate_system& trg_system,
                    const ia_coordinate& src_coordinate)
{
    LOG1("@%s, src_coordinate.x:%d, src_coordinate.y:%d",
        __FUNCTION__, src_coordinate.x, src_coordinate.y);

    return ia_coordinate_convert(&src_system, &trg_system, src_coordinate);
}
} NAMESPACE_DECLARATION_END
