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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_DEBUGCONTROL_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_DEBUGCONTROL_H_

#define KEY_DEBUG_DUMP "debug.fpipe.force.dump"
#define KEY_DEBUG_DUMP_COUNT "debug.fpipe.force.dump.count"

#define STR_ALLOCATE "pool allocate:"

#ifdef PIPE_MODULE_TAG
#undef PIPE_MODULE_TAG
#endif
#define PIPE_MODULE_TAG "MtkCam/CapturePipe_1"
#undef LOG_TAG
#define LOG_TAG "MtkCam/CapturePipe_1"

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_FEATURE_FEATURECORE_FEATUREPIPE_CAPTURE_DEBUGCONTROL_H_
