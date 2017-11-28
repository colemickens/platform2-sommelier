/*
 * Copyright (C) 2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#include "IPCCommon.h"
#include <iostream>
#include <string>

const char* Rockchip3AIpcCmdToString(IPC_CMD cmd) {
    static const char* gIpcCmdMapping[] = {
        "unknown",
        "IPC_3A_AIQ_INIT",
        "IPC_3A_AIQ_DEINIT",
        "IPC_3A_AIQ_AE_RUN",
        "IPC_3A_AIQ_AWB_RUN",
        "IPC_3A_AIQ_MISC_ISP_RUN",
        "IPC_3A_AIQ_STATISTICS_SET",
        "IPC_3A_AIQ_GET_VERSION",
    };

    unsigned int num = sizeof(gIpcCmdMapping) / sizeof(gIpcCmdMapping[0]);
    return cmd < num ? gIpcCmdMapping[cmd] : gIpcCmdMapping[0];
}
