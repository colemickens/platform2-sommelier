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

#define LOG_TAG "MtkCam/Cam3Device"
//
#include "MyUtils.h"
#include <mtkcam/main/hal/Cam3Device.h>
//
using NSCam::Cam3Device;
/******************************************************************************
 *
 ******************************************************************************/
static inline Cam3Device* getDevice(camera3_device const* device) {
  return Cam3Device::getDevice(device);
}

////////////////////////////////////////////////////////////////////////////////
//  Implementation of hw_device_t
////////////////////////////////////////////////////////////////////////////////
static int camera_close_device(hw_device_t* device) {
  if (!device) {
    return -EINVAL;
  }
  //
  return Cam3Device::getDevice(device)->i_closeDevice();
}

static hw_device_t const gHwDevice = {
    /** tag must be initialized to HARDWARE_DEVICE_TAG */
    .tag = HARDWARE_DEVICE_TAG,
    /** version number for hw_device_t */
    .version = CAMERA_DEVICE_API_VERSION_3_3,  // chrom only support 3_3 now
    /** reference to the module this device belongs to */
    .module = NULL,
    /** padding reserved for future use */
    .reserved = {0},
    /** Close this device */
    .close = camera_close_device,
};

////////////////////////////////////////////////////////////////////////////////
//  Implementation of camera3_device_ops
////////////////////////////////////////////////////////////////////////////////
static int camera_initialize(camera3_device const* device,
                             camera3_callback_ops_t const* callback_ops) {
  MERROR status = -ENODEV;
  //
  Cam3Device* const pDev = getDevice(device);
  if (pDev) {
    status = pDev->i_initialize(callback_ops);
  }
  return status;
}

static int camera_configure_streams(
    camera3_device const* device, camera3_stream_configuration_t* stream_list) {
  MERROR status = -ENODEV;
  //
  Cam3Device* const pDev = getDevice(device);
  if (pDev) {
    status = pDev->i_configure_streams(stream_list);
  }
  return status;
}

static camera_metadata_t const* camera_construct_default_request_settings(
    camera3_device const* device, int type) {
  Cam3Device* const pDev = getDevice(device);
  if (pDev) {
    return pDev->i_construct_default_request_settings(type);
  }
  return NULL;
}

static int camera_process_capture_request(camera3_device const* device,
                                          camera3_capture_request_t* request) {
  MERROR status = -ENODEV;
  //
  Cam3Device* const pDev = getDevice(device);
  if (pDev) {
    status = pDev->i_process_capture_request(request);
  }
  return status;
}

static void camera_dump(camera3_device const* device, int fd) {
  Cam3Device* const pDev = getDevice(device);
  if (pDev) {
    pDev->i_dump(fd);
  }
}

static int camera_flush(camera3_device const* device) {
  MERROR status = -ENODEV;
  //
  Cam3Device* const pDev = getDevice(device);
  if (pDev) {
    status = pDev->i_flush();
  }
  return status;
}

static camera3_device_ops const gCameraDevOps = {
#define OPS(name) .name = camera_##name
    OPS(initialize),
    OPS(configure_streams),
    .register_stream_buffers = NULL,
    OPS(construct_default_request_settings),
    OPS(process_capture_request),
    .get_metadata_vendor_tag_ops = NULL,
    OPS(dump),
    OPS(flush),
#undef OPS
    .reserved = {0},
};

/******************************************************************************
 *
 ******************************************************************************/
Cam3Device::Cam3Device() : mpModuleCallbacks(NULL) {
  MY_LOGD("ctor");
  ::memset(&mDevice, 0, sizeof(mDevice));
  mDevice.priv = this;
  mDevice.common = gHwDevice;
  mDevice.ops = &mDeviceOps;
  mDeviceOps = gCameraDevOps;
  //
}

/******************************************************************************
 *
 ******************************************************************************/
void Cam3Device::onLastStrongRef(const void* /*id*/) {
  MY_LOGD("");
}
