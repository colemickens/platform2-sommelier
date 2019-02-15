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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_PIPELOG_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_PIPELOG_H_

#ifndef PIPE_MODULE_TAG
#error "Must define PIPE_MODULE_TAG before include PipeLog.h"
#endif
#ifndef PIPE_CLASS_TAG
#error "Must define PIPE_CLASS_TAG before include PipeLog.h"
#endif

#undef LOG_TAG
#define LOG_TAG PIPE_MODULE_TAG "/" PIPE_CLASS_TAG
#include <mtkcam/utils/std/Log.h>

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_COMMON_INCLUDE_PIPELOG_H_
