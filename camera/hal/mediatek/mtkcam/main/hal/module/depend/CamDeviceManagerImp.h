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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_MODULE_DEPEND_CAMDEVICEMANAGERIMP_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_MODULE_DEPEND_CAMDEVICEMANAGERIMP_H_
//
#include <memory>
#include "CamDeviceManagerBase.h"

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *
 ******************************************************************************/
class CamDeviceManagerImp : public CamDeviceManagerBase {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Implementations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                        Instantiation.
  CamDeviceManagerImp();

 protected:  ////                        Operations.
  virtual MERROR validateOpenLocked(int32_t i4OpenId,
                                    uint32_t device_version) const;

  virtual int32_t enumDeviceLocked();

  virtual MERROR attachDeviceLocked(std::shared_ptr<ICamDevice> pDevice,
                                    uint32_t device_version);

  virtual MERROR detachDeviceLocked(std::shared_ptr<ICamDevice> pDevice);
};

/******************************************************************************
 *
 ******************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_MAIN_HAL_MODULE_DEPEND_CAMDEVICEMANAGERIMP_H_
