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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_APPREQUESTPARSER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_APPREQUESTPARSER_H_

#include <impl/types.h>
#include <mtkcam/pipeline/model/types.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/**
 * Parse a given request, and convert it to a convenient way.
 *
 * @param[out] out: a parsed request.
 *  Callers must ensure it's a non-nullptr.
 *  Callee is in charge of allocating memories for its inner pointers (i.e. for
 *  its data members in form of pointers), if any.
 *
 * @param[in] in: a given original request.
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
auto VISIBILITY_PUBLIC parseAppRequest(ParsedAppRequest* out,
                                       UserRequestParams const* in) -> int;

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_APPREQUESTPARSER_H_
