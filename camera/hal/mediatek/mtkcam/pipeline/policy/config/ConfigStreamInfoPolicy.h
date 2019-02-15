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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_CONFIG_CONFIGSTREAMINFOPOLICY_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_CONFIG_CONFIGSTREAMINFOPOLICY_H_
//
#include <memory>
#include <mtkcam/pipeline/policy/IConfigStreamInfoPolicy.h>
#include <mtkcam/pipeline/utils/streaminfo/ImageStreamInfo.h>
#include <mtkcam/pipeline/utils/streaminfo/MetaStreamInfo.h>
#include <mtkcam/utils/hw/HwInfoHelper.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace policy {

/**
 * pass1 meta stream info builder.
 */
class P1MetaStreamInfoBuilder : public NSCam::v3::Utils::MetaStreamInfoBuilder {
 protected:
  size_t mIndex = 0;

 public:
  explicit P1MetaStreamInfoBuilder(size_t index)
      : MetaStreamInfoBuilder(), mIndex(index) {}

 public:
  virtual auto setP1AppMetaDynamic_Default() -> P1MetaStreamInfoBuilder&;
  virtual auto setP1HalMetaDynamic_Default() -> P1MetaStreamInfoBuilder&;
  virtual auto setP1HalMetaControl_Default() -> P1MetaStreamInfoBuilder&;
};

/**
 * pass1 image stream info builder.
 */
class P1ImageStreamInfoBuilder
    : public NSCam::v3::Utils::ImageStreamInfoBuilder {
 protected:
  size_t mIndex = 0;
  std::shared_ptr<NSCamHW::HwInfoHelper> mHwInfoHelper = nullptr;

 public:
  P1ImageStreamInfoBuilder(size_t index,
                           std::shared_ptr<NSCamHW::HwInfoHelper> pHwInfoHelper)
      : ImageStreamInfoBuilder(), mIndex(index), mHwInfoHelper(pHwInfoHelper) {}

 public:
  virtual auto setP1Imgo_Default(size_t maxBufNum,
                                 P1HwSetting const& rP1HwSetting)
      -> P1ImageStreamInfoBuilder&;

  virtual auto setP1Rrzo_Default(size_t maxBufNum,
                                 P1HwSetting const& rP1HwSetting)
      -> P1ImageStreamInfoBuilder&;

  virtual auto setP1Lcso_Default(size_t maxBufNum) -> P1ImageStreamInfoBuilder&;

  virtual auto setP1Rsso_Default(size_t maxBufNum,
                                 P1HwSetting const& rP1HwSetting)
      -> P1ImageStreamInfoBuilder&;

 protected:
  virtual auto toBufPlanes(size_t stride, MINT imgFormat, MSize const& imgSize)
      -> IImageStreamInfo::BufPlanes_t;
};

/******************************************************************************
 *
 ******************************************************************************/
};  // namespace policy
};  // namespace pipeline
};  // namespace v3
};  // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_POLICY_CONFIG_CONFIGSTREAMINFOPOLICY_H_
