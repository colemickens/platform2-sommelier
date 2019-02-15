/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include "camera/hal/mediatek/mtkcam/pipeline/hwnode/p2/P2_Info.h"

namespace P2 {

#include <mtkcam/utils/debug/P2_DebugControl.h>
#define P2_CLASS_TAG P2Info
#define P2_TRACE TRACE_P2_INFO
#include "P2_LogHeader.h"

P2ConfigParam::P2ConfigParam() {}

P2ConfigParam::P2ConfigParam(const P2Info& info) : mP2Info(info) {}

P2InitParam::P2InitParam() {}

P2InitParam::P2InitParam(const P2Info& info) : mP2Info(info) {}

}  // namespace P2
