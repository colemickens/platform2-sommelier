/*
 * Copyright (C) 2017-2018 Intel Corporation.
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

#include <linux/videodev2.h>
#include "NodeTypes.h"

namespace android {
namespace camera2 {

enum v4l2_memory getDefaultMemoryType(IPU3NodeNames node)
{
    /*
     * All nodes use V4L2_MEMORY_DMABUF memory.
     */
    switch (node) {
    case ISYS_NODE_RAW:
    case IMGU_NODE_PARAM:
    case IMGU_NODE_STAT:
    case IMGU_NODE_INPUT:
    case IMGU_NODE_VF_PREVIEW:
    case IMGU_NODE_PV_PREVIEW:
    case IMGU_NODE_STILL:
    case IMGU_NODE_VIDEO:
        return V4L2_MEMORY_DMABUF;
    default:
        return V4L2_MEMORY_USERPTR;
    }
}

}
}
