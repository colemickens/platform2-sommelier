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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_ADAPTER_INCLUDE_IMPL_IHALDEVICEADAPTER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_ADAPTER_INCLUDE_IMPL_IHALDEVICEADAPTER_H_
//
#include <memory>
#include <string>
#include <vector>
/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {
namespace v3 {
namespace pipeline {
namespace model {

class IHalDeviceAdapter {
 public:  ////    Interfaces.
  static auto create(int32_t id) -> std::shared_ptr<IHalDeviceAdapter>;

  virtual auto open() -> bool = 0;
  virtual auto close() -> void = 0;

  virtual auto powerOn() -> bool = 0;
  virtual auto powerOff() -> void = 0;

  virtual auto getPhysicalSensorId(std::vector<int32_t>* rvSensorId) const
      -> bool = 0;
  virtual ~IHalDeviceAdapter() {}
};

};      // namespace model
};      // namespace pipeline
};      // namespace v3
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_PIPELINE_MODEL_ADAPTER_INCLUDE_IMPL_IHALDEVICEADAPTER_H_
