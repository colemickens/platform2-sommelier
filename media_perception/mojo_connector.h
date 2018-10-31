// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PERCEPTION_MOJO_CONNECTOR_H_
#define MEDIA_PERCEPTION_MOJO_CONNECTOR_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <base/bind.h>
#include <base/single_thread_task_runner.h>
#include <base/threading/thread.h>
#include <mojo/edk/embedder/embedder.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "media_perception/device_management.pb.h"
#include "media_perception/media_perception_service_impl.h"
#include "media_perception/producer_impl.h"
#include "media_perception/receiver_impl.h"
#include "media_perception/video_capture_service_client.h"
#include "mojom/media_perception_service.mojom.h"

namespace mri {

class MojoConnector {
 public:
  MojoConnector();
  ~MojoConnector() {}
  // Uses a file descriptor to establish a Mojo connection.
  void ReceiveMojoInvitationFileDescriptor(int fd_int);

  // Set a shared pointer member variable of the video capture service client
  // object.
  void SetVideoCaptureServiceClient(
      std::shared_ptr<VideoCaptureServiceClient> video_capture_service_client);

  // Use the Mojo connector to ensure the video capture servicec is started in
  // Chrome and get access to the video capture service Mojo API.
  void ConnectToVideoCaptureService();

  // Check the connection state.
  bool IsConnectedToVideoCaptureService();

  // Get the list of video devices from the video capture service.
  void GetDevices(
      const VideoCaptureServiceClient::GetDevicesCallback& callback);

  // Attempts to acquire exclusive access to a video device. Note that this does
  // not block another client of the video capture service from taking over
  // access on this device, which would disconnect this client.
  void SetActiveDevice(
      std::string device_id,
      const VideoCaptureServiceClient::SetActiveDeviceCallback& callback);

  // Starts video capture on the active device.
  void StartVideoCapture(const VideoStreamParams& capture_format,
                         std::function<void(uint64_t timestamp_in_microseconds,
                                            const uint8_t* data, int data_size)>
                             frame_handler);

  // Stops video capture on the active device.
  void StopVideoCapture();

  // Creates a new virtual device that frames can be fed into.
  void CreateVirtualDevice(
      const VideoDevice& video_device,
      std::shared_ptr<ProducerImpl> producer_impl,
      const VideoCaptureServiceClient::VirtualDeviceCallback& callback);

  void PushFrameToVirtualDevice(std::shared_ptr<ProducerImpl> producer_impl,
                                base::TimeDelta timestamp,
                                std::unique_ptr<const uint8_t[]> data,
                                int data_size, PixelFormat pixel_format,
                                int frame_width, int frame_height);

 private:
  // Returns a string with an obfuscated device id based on a counter.
  std::string GetObfuscatedDeviceId(
      const std::string& device_id,
      const std::string& display_name);

  // Handler for when the Mojo connection is closed or errors out.
  void OnConnectionErrorOrClosed();

  // Handler for when device pointer connection is closed or errors out.
  void OnDeviceFactoryConnectionErrorOrClosed();

  void AcceptConnectionOnIpcThread(base::ScopedFD fd);

  void ConnectToVideoCaptureServiceOnIpcThread();

  void GetDevicesOnIpcThread(
      const VideoCaptureServiceClient::GetDevicesCallback& callback);

  void OnDeviceInfosReceived(
      const VideoCaptureServiceClient::GetDevicesCallback& callback,
      std::vector<media::mojom::VideoCaptureDeviceInfoPtr> infos);

  void SetActiveDeviceOnIpcThread(
      std::string device_id,
      const VideoCaptureServiceClient::SetActiveDeviceCallback& callback);

  void OnSetActiveDeviceCallback(
      const VideoCaptureServiceClient::SetActiveDeviceCallback& callback,
      video_capture::mojom::DeviceAccessResultCode code);

  void StartVideoCaptureOnIpcThread(const VideoStreamParams& capture_format);

  void StopVideoCaptureOnIpcThread();

  void CreateVirtualDeviceOnIpcThread(
      const VideoDevice& video_device,
      std::shared_ptr<ProducerImpl> producer_impl,
      const VideoCaptureServiceClient::VirtualDeviceCallback& callback);

  void PushFrameToVirtualDeviceOnIpcThread(
      std::shared_ptr<ProducerImpl> producer_impl, base::TimeDelta timestamp,
      std::unique_ptr<const uint8_t[]> data, int data_size,
      PixelFormat pixel_format, int frame_width, int frame_height);

  // Separate thread for doing IPC via Mojo because Mojo is asynchronous
  // by default.
  base::Thread ipc_thread_;

  // Stores pointer to the video capture service client object.
  std::shared_ptr<VideoCaptureServiceClient> video_capture_service_client_;

  // Implementation for the media perception service Mojo interface.
  std::unique_ptr<MediaPerceptionServiceImpl> media_perception_service_impl_;

  // Entry point Mojo object for talking to the video capture service API.
  video_capture::mojom::DeviceFactoryPtr device_factory_;

  // Provides interface to an open device.
  video_capture::mojom::DevicePtr active_device_;

  int unique_device_counter_;

  // Maps unique ids for devices (device_id + display_name) to an obfuscated
  // device id, which is generated by a counter.
  std::map<
      std::string /* device_id + display_name */,
      std::string /* obfuscated device_id */> unique_id_map_;

  // Map to store obfuscated device ids and their associated real device ids.
  // Makes it so that clients of the service do not know or need to know the
  // real id of connected devices.
  std::map<
      std::string /* obfuscated device_id */,
      std::string /* device_id */> obfuscated_device_id_map_;

  // Provides interface for receiving frames from the video capture service.
  ReceiverImpl receiver_impl_;

  std::mutex vcs_connection_state_mutex_;
  bool is_connected_to_vcs_ = false;
};

}  // namespace mri

#endif  // MEDIA_PERCEPTION_MOJO_CONNECTOR_H_
