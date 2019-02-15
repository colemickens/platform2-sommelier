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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_MAIN_HAL_ICAMDEVICE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_MAIN_HAL_ICAMDEVICE_H_
//
#include <hardware/camera_common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *
 ******************************************************************************/
class ICamDeviceManager;

/******************************************************************************
 *
 ******************************************************************************/
class ICamDevice {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Destructor.
  //  Disallowed to directly delete a raw pointer.
  virtual ~ICamDevice() {}

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  virtual char const* getDevName() const = 0;

  virtual int32_t getOpenId() const = 0;

  virtual hw_device_t const* get_hw_device() const = 0;

  virtual void set_hw_module(hw_module_t const* module) = 0;

  virtual void set_module_callbacks(
      camera_module_callbacks_t const* callbacks) = 0;

  virtual void setDeviceManager(ICamDeviceManager* manager) = 0;
};

/***************************************************************************
 *
 ***************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_MAIN_HAL_ICAMDEVICE_H_
