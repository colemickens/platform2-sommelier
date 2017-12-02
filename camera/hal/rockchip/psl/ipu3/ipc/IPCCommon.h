/*
 * Copyright (C) 2017 Intel Corporation
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

#ifndef PSL_IPU3_IPC_IPCCOMMON_H_
#define PSL_IPU3_IPC_IPCCOMMON_H_

#include <ia_cmc_types.h>
#include <ia_types.h>
#include <ia_aiq_types.h>
#include <IPU3AICCommon.h>

#define IPC_MATCHING_KEY 0x56 // the value is randomly chosen
#define IPC_REQUEST_HEADER_USED_NUM 2

enum IPC_CMD {
    IPC_3A_AIC_INIT = 1,
    IPC_3A_AIC_RUN,
    IPC_3A_AIC_RESET,
    IPC_3A_AIC_GETAICVERSION,
    IPC_3A_AIC_GETAICCONFIG,
    IPC_3A_AIQ_INIT,
    IPC_3A_AIQ_DEINIT,
    IPC_3A_AIQ_AE_RUN,
    IPC_3A_AIQ_AF_RUN,
    IPC_3A_AIQ_AWB_RUN,
    IPC_3A_AIQ_GBCE_RUN,
    IPC_3A_AIQ_PA_RUN,
    IPC_3A_AIQ_SA_RUN,
    IPC_3A_AIQ_GET_AIQ_DATA,
    IPC_3A_AIQ_STATISTICS_SET,
    IPC_3A_AIQ_GET_VERSION,
    IPC_3A_CMC_INIT,
    IPC_3A_CMC_DEINIT,
    IPC_3A_EXC_ANALOG_GAIN_TO_SENSOR,
    IPC_3A_EXC_SENSOR_TO_ANALOG_GAIN,
    IPC_3A_MKN_INIT,
    IPC_3A_MKN_UNINIT,
    IPC_3A_MKN_PREPARE,
    IPC_3A_MKN_ENABLE,
    IPC_3A_COORDINATE_COVERT,
    IPC_3A_COORDINATE_FACES
};

#define MAX_IA_BINARY_DATA_SIZE 800000
struct ia_binary_data_mod
{
    unsigned int size;
    char data[MAX_IA_BINARY_DATA_SIZE];
};

const char* Intel3AIpcCmdToString(IPC_CMD cmd);

#endif // PSL_IPU3_IPC_IPCCOMMON_H_