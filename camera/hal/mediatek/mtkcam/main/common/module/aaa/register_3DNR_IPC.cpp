/*
 * Copyright (C) 2019 Mediatek Corporation.
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

#define LOG_TAG "mtkcam-module"
//
#include <mtkcam/main/common/module/local.h>
//
#include <libeis/MTKEis.h>
//

namespace IPC3DNR {

extern "C" MTKEis* createInstance_3DNR_Client(const MINT32 sensor_idx,
                                              const char* user_name);

}
REGISTER_MTKCAM_MODULE(MTKCAM_MODULE_ID_AAA_3DNR_IPC,
                       IPC3DNR::createInstance_3DNR_Client);
