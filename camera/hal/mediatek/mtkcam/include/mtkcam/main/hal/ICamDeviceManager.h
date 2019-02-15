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

#ifndef CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_MAIN_HAL_ICAMDEVICEMANAGER_H_
#define CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_MAIN_HAL_ICAMDEVICEMANAGER_H_

#include <hardware/camera_common.h>
#include <mtkcam/def/common.h>
//

/******************************************************************************
 *
 ******************************************************************************/
namespace NSCam {

/******************************************************************************
 *
 ******************************************************************************/
class ICamDevice;

/******************************************************************************
 *
 ******************************************************************************/
class ICamDeviceManager {
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Instantiation.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 protected:  ////                        Destructor.
  //  Disallowed to directly delete a raw pointer.
  virtual ~ICamDeviceManager() {}

  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  //  Interfaces.
  //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 public:  ////
  virtual status_t open(hw_device_t** device,
                        hw_module_t const* module,
                        char const* name,
                        uint32_t device_version = 0) = 0;

  virtual status_t close() = 0;

  /**
   * getNumberOfDevices:
   *
   * Returns the number of camera devices accessible through the camera
   * module.  The camera devices are numbered 0 through N-1, where N is the
   * value returned by this call. The name of the camera device for open() is
   * simply the number converted to a string. That is, "0" for camera ID 0,
   * "1" for camera ID 1.
   *
   * The value here must be static, and cannot change after the first call to
   * this method
   */
  virtual int32_t getNumberOfDevices() = 0;

  /**
   * getDeviceInfo:
   *
   * Return the static information for a given camera device. This information
   * may not change for a camera device.
   *
   */
  virtual status_t getDeviceInfo(int deviceId, camera_info* rInfo) = 0;

  /**
   * set_callbacks:
   *
   * Provide callback function pointers to the HAL module to inform framework
   * of asynchronous camera module events. The framework will call this
   * function once after initial camera HAL module load, after the
   * get_number_of_cameras() method is called for the first time, and before
   * any other calls to the module.
   *
   * Version information (based on camera_module_t.common.module_api_version):
   *
   *  CAMERA_MODULE_API_VERSION_1_0, CAMERA_MODULE_API_VERSION_2_0:
   *
   *    Not provided by HAL module. Framework may not call this function.
   *
   *  CAMERA_MODULE_API_VERSION_2_1:
   *
   *    Valid to be called by the framework.
   *
   */
  virtual status_t setCallbacks(camera_module_callbacks_t const* callbacks) = 0;
};

/***************************************************************************
 *
 ***************************************************************************/
ICamDeviceManager* getCamDeviceManager();

/***************************************************************************
 *
 ***************************************************************************/
};      // namespace NSCam
#endif  // CAMERA_HAL_MEDIATEK_MTKCAM_INCLUDE_MTKCAM_MAIN_HAL_ICAMDEVICEMANAGER_H_
