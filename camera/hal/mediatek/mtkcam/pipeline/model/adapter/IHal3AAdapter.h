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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_ADAPTER_IHAL3AADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_ADAPTER_IHAL3AADAPTER_H_

#include <memory>
#include <string>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

class IHal3AAdapter {
 public:  ////    Interfaces.
  static auto create(int32_t id, std::string const& name)
      -> std::shared_ptr<IHal3AAdapter>;

  virtual auto notifyPowerOn() -> bool = 0;

  virtual auto notifyPowerOff() -> bool = 0;
};

};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_ADAPTER_IHAL3AADAPTER_H_
