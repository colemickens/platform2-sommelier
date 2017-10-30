/*
 * Copyright (C) 2017-2018 Intel Corporation
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

#ifndef NODETYPES_H_
#define NODETYPES_H_


namespace android {
namespace camera2 {

/**
 * This enumeration lists the V4L2 nodes exposed by the InputSystem
 */
enum IPU3NodeNames {
    IMGU_NODE_NULL =            0,
    IMGU_NODE_VF_PREVIEW =      1 << 1,
    IMGU_NODE_PV_PREVIEW =      1 << 2,
    IMGU_NODE_VIDEO =           1 << 3,
    IMGU_NODE_STILL =           1 << 4,
    IMGU_NODE_RAW =             1 << 5,
    IMGU_NODE_PARAM =           1 << 6,
    IMGU_NODE_STAT =            1 << 7,
    IMGU_NODE_INPUT =           1 << 8,
    ISYS_NODE_RAW =             1 << 9
};

enum v4l2_memory getDefaultMemoryType(IPU3NodeNames node);

}  // namespace camera2
}  // namespace android

#endif /* NODETYPES_H_ */
