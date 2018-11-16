// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_VIDEO_CAPTURE_SERVICE_CLIENT_H_
#define MEDIA_PERCEPTION_VIDEO_CAPTURE_SERVICE_CLIENT_H_

// This header needs to be buildable from both Google3 and outside, so it cannot
// rely on Google3 dependencies.

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mri {

enum DeviceAccessResultCode {
  RESULT_UNKNOWN,
  NOT_INITIALIZED,
  SUCCESS,
  ERROR_DEVICE_NOT_FOUND
};

enum PixelFormat { FORMAT_UNKNOWN, I420, MJPEG };

struct CaptureFormat {
  int frame_width;
  int frame_height;
  float frame_rate;  // In frames per second.
  PixelFormat pixel_format;
};

struct VideoDevice {
  // Obfuscated unique id for this video device.
  std::string id;

  // Human readable name of the video device.
  std::string display_name;

  // Model id string.
  std::string model_id;

  // List of supported formats for the camera.
  std::vector<CaptureFormat> supported_formats;
};

// Provides the interface definition for the rtanalytics library to interact
// with the Chrome Video Capture Service.
class VideoCaptureServiceClient {
 public:
  using GetDevicesCallback = std::function<void(std::vector<VideoDevice>)>;
  using SetActiveDeviceCallback = std::function<void(DeviceAccessResultCode)>;
  using VirtualDeviceCallback = std::function<void(VideoDevice)>;
  using FrameHandler =
      std::function<void(uint64_t timestamp_us, const uint8_t* data,
                         int data_size, int frame_width, int frame_height)>;

  virtual ~VideoCaptureServiceClient() {}

  // Connects to the Video Capture Service over Mojo IPC.
  virtual bool Connect() = 0;

  // Check if the service is connected.
  virtual bool IsConnected() = 0;

  // Gets a list of video devices available.
  virtual void GetDevices(const GetDevicesCallback& callback) = 0;

  // Sets the active device to be opened by the Video Capture Service.
  // Return value indicates success or failure of setting the active device.
  virtual void SetActiveDevice(const std::string& device_id,
                               const SetActiveDeviceCallback& callback) = 0;

  // Starts video capture on the active device. Frames will be forwarded to
  // the frame handler.
  virtual void StartVideoCapture(const CaptureFormat& capture_format) = 0;

  // Interface for creating a virtual device with a set of parameters.
  virtual void CreateVirtualDevice(const VideoDevice& video_device,
                                   const VirtualDeviceCallback& callback) = 0;

  // Pushes frame data to the specified virtual device, if opened.
  virtual void PushFrameToVirtualDevice(const std::string& device_id,
                                        uint64_t timestamp_us,
                                        std::unique_ptr<const uint8_t[]> data,
                                        int data_size, PixelFormat pixel_format,
                                        int frame_width, int frame_height) = 0;

  // Closes the specified virtual device.
  virtual void CloseVirtualDevice(const std::string& device_id) = 0;

  // Stops video capture from the active device.
  virtual void StopVideoCapture() = 0;

  virtual void SetFrameHandler(FrameHandler handler) {
    frame_handler_ = std::move(handler);
  }

 protected:
  // Handler for processing input frames.
  FrameHandler frame_handler_;
};

}  // namespace mri

#endif  // MEDIA_PERCEPTION_VIDEO_CAPTURE_SERVICE_CLIENT_H_
