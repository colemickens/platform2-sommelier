/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef USB_CAMERA_CLIENT_H_
#define USB_CAMERA_CLIENT_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <base/threading/thread_checker.h>
#include <hardware/camera3.h>
#include <hardware/hardware.h>

#include "usb/camera_metadata.h"
#include "usb/common_types.h"
#include "usb/v4l2_camera_device.h"

namespace arc {

// CameraClient class is not thread-safe. Constructor, OpenDevice, and
// CloseDevice must be called on the same thread. Camera v3 Device
// Operations must be called on the same thread. But OpenDevice and device
// operations can be called on different threads.
class CameraClient {
 public:
  // id is used to distinguish cameras. 0 <= id < number of cameras.
  CameraClient(int id,
               const std::string& device_path,
               const camera_metadata_t& static_info,
               const hw_module_t* module,
               hw_device_t** hw_device);
  ~CameraClient();

  // Camera Device Operations from CameraHal.
  int OpenDevice();
  int CloseDevice();

  int GetId() const { return id_; }

  // Camera v3 Device Operations (see <hardware/camera3.h>)
  int Initialize(const camera3_callback_ops_t* callback_ops);
  int ConfigureStreams(camera3_stream_configuration_t* stream_list);
  // |type| is camera3_request_template_t in camera3.h.
  const camera_metadata_t* ConstructDefaultRequestSettings(int type);
  int ProcessCaptureRequest(camera3_capture_request_t* request);
  void Dump(int fd);
  int Flush(const camera3_device_t* dev);

 private:
  // Camera device id.
  const int id_;

  // Camera device path.
  const std::string device_path_;

  // Camera device handle returned to framework for use.
  camera3_device_t camera3_device_;

  // Use to check the constructor, OpenDevice, and CloseDevice are called on the
  // same thread.
  base::ThreadChecker thread_checker_;

  // Use to check camera v3 device operations are called on the same thread.
  base::ThreadChecker ops_thread_checker_;

  // Delegate to communicate with camera device.
  std::unique_ptr<V4L2CameraDevice> device_;

  // Metadata containing persistent camera characteristics
  CameraMetadata metadata_;

  DISALLOW_COPY_AND_ASSIGN(CameraClient);
};

}  // namespace arc

#endif  // USB_CAMERA_CLIENT_H_
