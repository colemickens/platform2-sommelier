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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_MAIN_HAL_CAM3DEVICE_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_MAIN_HAL_CAM3DEVICE_H_
//
#include <memory>
#include <string>
//
#include <hardware/camera3.h>
#include "ICamDevice.h"
#include <mtkcam/def/common.h>

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *
 ******************************************************************************/
class Cam3Device : public ICamDevice {
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Definitions.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Definitions.
  typedef status_t status_t;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Data Members.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Data Members.
  camera_module_callbacks_t const* mpModuleCallbacks;
  camera3_device mDevice;
  camera3_device_ops mDeviceOps;  //  which is pointed to by mDevice.ops.

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Attributes.
  //
  static inline Cam3Device* getDevice(camera3_device const* device) {
    Cam3Device* ret_dev = reinterpret_cast<Cam3Device*>((device)->priv);
    return (NULL == device) ? NULL : ret_dev;
  }

  static inline Cam3Device* getDevice(hw_device_t* device) {
    return getDevice((camera3_device const*)device);
  }

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Operations.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                    Instantiation.
  virtual ~Cam3Device() {}
  Cam3Device();

  virtual void onLastStrongRef(const void* id);

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////                    Initialization.
  virtual status_t i_closeDevice() = 0;

  /**
   * One-time initialization to pass framework callback function pointers to
   * the HAL. Will be called once after a successful open() call, before any
   * other functions are called on the camera3_device_ops structure.
   *
   * Return values:
   *
   *  0:     On successful initialization
   *
   * -ENODEV: If initialization fails. Only close() can be called successfully
   *          by the framework after this.
   */
  virtual status_t i_initialize(camera3_callback_ops const* callback_ops) = 0;

  /**
   * Uninitialize the device resources owned by this object. Note that this
   * is *not* done in the destructor.
   *
   * This may be called at any time, although the call may block until all
   * in-flight captures have completed (all results returned, all buffers
   * filled). After the call returns, no more callbacks are allowed.
   */
  virtual status_t i_uninitialize() = 0;

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 public:  ////                    Stream management
  virtual status_t i_configure_streams(
      camera3_stream_configuration_t* stream_list) = 0;

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 public:  ////                    Request creation and submission
  virtual camera_metadata const* i_construct_default_request_settings(
      int type) = 0;

  virtual status_t i_process_capture_request(
      camera3_capture_request_t* request) = 0;

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 public:  ////                    Miscellaneous methods
  virtual status_t i_flush() = 0;

  virtual void i_dump(int fd) = 0;

  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  ICamDevice Interfaces.
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  virtual hw_device_t const* get_hw_device() const { return &mDevice.common; }

  virtual void set_hw_module(hw_module_t const* module) {
    mDevice.common.module = const_cast<hw_module_t*>(module);
  }

  virtual void set_module_callbacks(
      camera_module_callbacks_t const* callbacks) {
    mpModuleCallbacks = callbacks;
  }
};
};  // namespace NSCam

/****************************************************************************
 *
 ****************************************************************************/
std::shared_ptr<NSCam::Cam3Device> VISIBILITY_PUBLIC
createCam3Device(std::string const s8ClientAppMode, int32_t const i4OpenId);

#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_MAIN_HAL_CAM3DEVICE_H_
