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

#ifndef PSL_RKISP1_IPC_IPCCOMMON_H_
#define PSL_RKISP1_IPC_IPCCOMMON_H_

#include <ia_types.h>
#include <rk_aiq_types.h>

#define IPC_MATCHING_KEY 0x56 // the value is randomly chosen
#define IPC_REQUEST_HEADER_USED_NUM 2

enum IPC_CMD {
    IPC_3A_AIQ_INIT = 1,
    IPC_3A_AIQ_DEINIT,
    IPC_3A_AIQ_AE_RUN,
    IPC_3A_AIQ_AWB_RUN,
    IPC_3A_AIQ_MISC_ISP_RUN,
    IPC_3A_AIQ_STATISTICS_SET,
    IPC_3A_AIQ_GET_VERSION,
};

const char* Rockchip3AIpcCmdToString(IPC_CMD cmd);

#endif // PSL_RKISP1_IPC_IPCCOMMON_H_
