/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_V4L2_CAMERA_DEVICE_H_
#define HAL_USB_V4L2_CAMERA_DEVICE_H_

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/synchronization/lock.h>

#include "cros-camera/timezone.h"
#include "hal/usb/common_types.h"

namespace arc {

// The class is thread-safe.
class V4L2CameraDevice {
 public:
  V4L2CameraDevice();
  explicit V4L2CameraDevice(const DeviceInfo& device_info);
  virtual ~V4L2CameraDevice();

  // Connect camera device with |device_path|. Return 0 if device is opened
  // successfully. Otherwise, return -|errno|.
  int Connect(const std::string& device_path);

  // Disconnect camera device. This function is a no-op if the camera device
  // is not connected. If the stream is on, this function will also stop the
  // stream.
  void Disconnect();

  // Enable camera device stream. Setup captured frame with |width|x|height|
  // resolution, |pixel_format|, and |frame_rate|. Get frame buffer file
  // descriptors |fds| and |buffer_size|. |buffer_size| is the size allocated
  // for each buffer. The ownership of |fds| are transferred to the caller and
  // |fds| should be closed when done. Caller can memory map |fds| and should
  // unmap when done. Return 0 if device supports the format. Otherwise, return
  // -|errno|. This function should be called after Connect().
  int StreamOn(uint32_t width,
               uint32_t height,
               uint32_t pixel_format,
               float frame_rate,
               std::vector<base::ScopedFD>* fds,
               uint32_t* buffer_size);

  // Disable camera device stream. Return 0 if device disables stream
  // successfully. Otherwise, return -|errno|. This function is a no-op if the
  // stream is already stopped.
  int StreamOff();

  // Get next frame buffer from device. Device returns the corresponding
  // buffer with |buffer_id| and |data_size| bytes. |data_size| is how many
  // bytes used in the buffer for this frame. Return 0 if device gets the
  // buffer successfully. Otherwise, return -|errno|. Return -EAGAIN immediately
  // if next frame buffer is not ready. This function should be called after
  // StreamOn().
  int GetNextFrameBuffer(uint32_t* buffer_id, uint32_t* data_size);

  // Return |buffer_id| buffer to device. Return 0 if the buffer is returned
  // successfully. Otherwise, return -|errno|. This function should be called
  // after StreamOn().
  int ReuseFrameBuffer(uint32_t buffer_id);

  // TODO(henryhsu): GetDeviceSupportedFormats, GetCameraDeviceInfos, and the
  // private functions used by them can be changed to static.

  // Get all supported formats of device by |device_path|. This function can be
  // called without calling Connect().
  const SupportedFormats GetDeviceSupportedFormats(
      const std::string& device_path);

  // Get all camera devices information. This function can be called without
  // calling Connect().
  const DeviceInfos GetCameraDeviceInfos();

 private:
  std::vector<float> GetFrameRateList(int fd,
                                      uint32_t fourcc,
                                      uint32_t width,
                                      uint32_t height);
  const std::unordered_map<std::string, DeviceInfo> GetCameraDevicesByPattern(
      std::string pattern);

  // This is for suspend/resume feature. USB camera will be enumerated after
  // device resumed. But camera device may not be ready immediately.
  int RetryDeviceOpen(const std::string& device_path, int flags);

  // External camera is the only one camera device of /dev/video* which is not
  // in |internal_devices_|. Ruturns <VID:PID, device_info> if external camera
  // is found. Otherwise, returns empty strings.
  std::pair<std::string, DeviceInfo> FindExternalCamera();

  // Get power frequency supported from device.
  PowerLineFrequency GetPowerLineFrequency(int fd);

  // Set power frequency supported from device.
  int SetPowerLineFrequency(PowerLineFrequency setting);

  // The number of video buffers we want to request in kernel.
  const int kNumVideoBuffers = 4;

  // The opened device fd.
  base::ScopedFD device_fd_;

  // StreamOn state
  bool stream_on_;

  // True if the buffer is used by client after GetNextFrameBuffer().
  std::vector<bool> buffers_at_client_;

  // Keep internal camera devices to distinguish external camera.
  // First index is VID:PID and second index is the device info.
  std::unordered_map<std::string, DeviceInfo> internal_devices_;

  const DeviceInfo device_info_;

  // Since V4L2CameraDevice may be called on different threads, this is used to
  // guard all variables.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(V4L2CameraDevice);
};

}  // namespace arc

#endif  // HAL_USB_V4L2_CAMERA_DEVICE_H_
