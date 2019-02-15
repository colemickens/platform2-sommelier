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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_RESULTUPDATEHELPER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_RESULTUPDATEHELPER_H_

#include <memory>
#include <mtkcam/pipeline/model/IPipelineModelCallback.h>

#include <impl/types.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/**
 * Help to update one-shot result metadata.
 *
 * @param[out] out: n/a.
 *
 * @param[in] in: input parameters.
 *
 * @return
 *
 */
auto VISIBILITY_PUBLIC
ResultUpdateHelper(std::weak_ptr<IPipelineModelCallback> const& rpCallback,
                   uint32_t requestNo,
                   std::shared_ptr<IMetaStreamBuffer> resultMeta,
                   bool isLastPartial = false) -> void;

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_RESULTUPDATEHELPER_H_
