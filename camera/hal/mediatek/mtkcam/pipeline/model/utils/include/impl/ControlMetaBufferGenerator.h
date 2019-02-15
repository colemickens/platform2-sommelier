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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_CONTROLMETABUFFERGENERATOR_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_CONTROLMETABUFFERGENERATOR_H_

#include <impl/types.h>
#include <memory>
#include <mtkcam/pipeline/utils/streambuf/StreamBuffers.h>
#include <vector>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

/**
 * Generate a control App meta stream buffer
 *
 * @param[out] out: a vector where a newly-generated meta stream buffer will be
 * added to.
 *
 * @param[in] pMetaStreamBuffer: the original control App meta stream buffer
 *  associating with pOriginalAppControl.
 *
 * @param[in] pOriginalAppControl: the original control App metadata
 *  sent from the application/frameworks.
 *
 * @param[in] pAdditionalApp: additional App metadata.
 *  It could be a nullptr.
 *
 * @param[in] pInfo: control App meta stream info.
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
auto VISIBILITY_PUBLIC generateControlAppMetaBuffer(
    std::vector<std::shared_ptr<IMetaStreamBuffer>>* out,
    std::shared_ptr<IMetaStreamBuffer> const pMetaStreamBuffer,
    IMetadata const* pOriginalAppControl,
    IMetadata const* pAdditionalApp,
    std::shared_ptr<const IMetaStreamInfo> pInfo) -> int;

/**
 * Generate a control Hal meta stream buffer
 *
 * If pMetaStreamBuffer is not a nullptr, append the given additional metadata
 * (i.e. pAdditionalApp) to pMetaStreamBuffer; otherwise allocate a new one in
 * form of the given original (i.e. pOriginalAppControl) + the additional (i.e.
 * pAdditionalApp).
 *
 * @param[out] out: a vector where a newly-generated meta stream buffer will be
 * added to.
 *
 * @param[in] pAdditionalHal: additional Hal metadata.
 *  If it's a nullptr, no meta stream buffer will be generated.
 *
 * @param[in] pInfo: control Hal meta stream info.
 *
 * @return
 *      0 indicates success; otherwise failure.
 */
auto VISIBILITY_PUBLIC generateControlHalMetaBuffer(
    std::vector<std::shared_ptr<NSCam::v3::Utils::HalMetaStreamBuffer>>* out,
    IMetadata const* pAdditionalHal,
    std::shared_ptr<const IMetaStreamInfo> pInfo) -> int;

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_UTILS_INCLUDE_IMPL_CONTROLMETABUFFERGENERATOR_H_
