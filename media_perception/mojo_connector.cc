// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_perception/mojo_connector.h"

#include <utility>
#include <vector>

namespace mri {

namespace {

DeviceAccessResultCode GetDeviceAccessResultCode(
    video_capture::mojom::DeviceAccessResultCode code) {
  switch (code) {
    case video_capture::mojom::DeviceAccessResultCode::NOT_INITIALIZED:
      return DeviceAccessResultCode::NOT_INITIALIZED;
    case video_capture::mojom::DeviceAccessResultCode::SUCCESS:
      return DeviceAccessResultCode::SUCCESS;
    case video_capture::mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND:
      return DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND;
  }
  return DeviceAccessResultCode::RESULT_UNKNOWN;
}

PixelFormat GetPixelFormatFromVideoCapturePixelFormat(
    media::mojom::VideoCapturePixelFormat format) {
  switch (format) {
    case media::mojom::VideoCapturePixelFormat::I420:
      return PixelFormat::I420;
    case media::mojom::VideoCapturePixelFormat::MJPEG:
      return PixelFormat::MJPEG;
    default:
      return PixelFormat::FORMAT_UNKNOWN;
  }
  return PixelFormat::FORMAT_UNKNOWN;
}

media::mojom::VideoCapturePixelFormat GetVideoCapturePixelFormatFromPixelFormat(
    PixelFormat pixel_format) {
  switch (pixel_format) {
    case PixelFormat::I420:
      return media::mojom::VideoCapturePixelFormat::I420;
    case PixelFormat::MJPEG:
      return media::mojom::VideoCapturePixelFormat::MJPEG;
    default:
      return media::mojom::VideoCapturePixelFormat::UNKNOWN;
  }
  return media::mojom::VideoCapturePixelFormat::UNKNOWN;
}

constexpr char kConnectorPipe[] = "mpp-connector-pipe";

}  // namespace

MojoConnector::MojoConnector() : ipc_thread_("IpcThread") {
  mojo::edk::Init();
  LOG(INFO) << "Starting IPC thread.";
  if (!ipc_thread_.StartWithOptions(
          base::Thread::Options(base::MessageLoop::TYPE_IO, 0))) {
    LOG(ERROR) << "Failed to start IPC Thread";
  }
  mojo::edk::InitIPCSupport(this, ipc_thread_.task_runner());
}

void MojoConnector::ReceiveMojoInvitationFileDescriptor(int fd_int) {
  base::ScopedFD fd(fd_int);
  if (!fd.is_valid()) {
    LOG(ERROR) << "FD is not valid.";
    return;
  }
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoConnector::AcceptConnectionOnIpcThread,
                            base::Unretained(this), base::Passed(&fd)));
}

void MojoConnector::OnConnectionErrorOrClosed() {
  LOG(ERROR) << "Connection error/closed received";
}

void MojoConnector::AcceptConnectionOnIpcThread(base::ScopedFD fd) {
  CHECK(ipc_thread_.task_runner()->BelongsToCurrentThread());
  mojo::edk::SetParentPipeHandle(
      mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(fd.release())));
  mojo::ScopedMessagePipeHandle child_pipe =
      mojo::edk::CreateChildMessagePipe(kConnectorPipe);
  if (!child_pipe.is_valid()) {
    LOG(ERROR) << "child_pipe is not valid";
  }
  ::chromeos::media_perception::mojom::ConnectorPtrInfo ptr_info(
      std::move(child_pipe), 0u);
  connector_.Bind(std::move(ptr_info), base::ThreadTaskRunnerHandle::Get());
  connector_.set_connection_error_handler(base::Bind(
      &MojoConnector::OnConnectionErrorOrClosed, base::Unretained(this)));
}

void MojoConnector::ConnectToVideoCaptureService() {
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&MojoConnector::ConnectToVideoCaptureServiceOnIpcThread,
                 base::Unretained(this)));
}

void MojoConnector::ConnectToVideoCaptureServiceOnIpcThread() {
  connector_->ConnectToVideoCaptureService(mojo::GetProxy(&device_factory_));
}

void MojoConnector::GetDevices(
    const VideoCaptureServiceClient::GetDevicesCallback& callback) {
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoConnector::GetDevicesOnIpcThread,
                            base::Unretained(this), callback));
}

void MojoConnector::GetDevicesOnIpcThread(
    const VideoCaptureServiceClient::GetDevicesCallback& callback) {
  device_factory_->GetDeviceInfos(base::Bind(
      &MojoConnector::OnDeviceInfosReceived, base::Unretained(this), callback));
}

void MojoConnector::OnDeviceInfosReceived(
    const VideoCaptureServiceClient::GetDevicesCallback& callback,
    mojo::Array<media::mojom::VideoCaptureDeviceInfoPtr> infos) {
  LOG(INFO) << "Got callback for device infos.";
  std::vector<VideoDevice> devices;
  for (const auto& capture_device : infos) {
    VideoDevice device;
    device.id = capture_device->descriptor->device_id;
    device.display_name = capture_device->descriptor->display_name;
    device.model_id = capture_device->descriptor->model_id;
    LOG(INFO) << "Device: " << device.display_name;
    for (const auto& capture_format : capture_device->supported_formats) {
      CaptureFormat supported_format;
      supported_format.frame_width = capture_format->frame_size->width;
      supported_format.frame_height = capture_format->frame_size->height;
      supported_format.frame_rate = capture_format->frame_rate;
      supported_format.pixel_format = GetPixelFormatFromVideoCapturePixelFormat(
          capture_format->pixel_format);
      device.supported_formats.push_back(supported_format);
    }
    devices.push_back(device);
  }
  callback(devices);
}

void MojoConnector::SetActiveDevice(
    std::string device_id,
    const VideoCaptureServiceClient::SetActiveDeviceCallback& callback) {
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoConnector::SetActiveDeviceOnIpcThread,
                            base::Unretained(this), device_id, callback));
}

void MojoConnector::SetActiveDeviceOnIpcThread(
    std::string device_id,
    const VideoCaptureServiceClient::SetActiveDeviceCallback& callback) {
  device_factory_->CreateDevice(
      device_id, mojo::GetProxy(&active_device_),
      base::Bind(&MojoConnector::OnSetActiveDeviceCallback,
                 base::Unretained(this), callback));
}

void MojoConnector::OnSetActiveDeviceCallback(
    const VideoCaptureServiceClient::SetActiveDeviceCallback& callback,
    video_capture::mojom::DeviceAccessResultCode code) {
  callback(GetDeviceAccessResultCode(code));
}

void MojoConnector::StartVideoCapture(
    const CaptureFormat& capture_format,
    std::function<void(uint64_t timestamp_in_microseconds, const uint8_t* data,
                       int data_size)>
        frame_handler) {
  LOG(INFO) << "Setting frame handler.";
  receiver_impl_.SetFrameHandler(std::move(frame_handler));
  // Mojo code to start video capture and pass frames to the frame handler.
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoConnector::StartVideoCaptureOnIpcThread,
                            base::Unretained(this), capture_format));
}

void MojoConnector::StartVideoCaptureOnIpcThread(
    const CaptureFormat& capture_format) {
  LOG(INFO) << "Starting video capture on ipc thread.";

  auto requested_settings = media::mojom::VideoCaptureParams::New();
  requested_settings->requested_format =
      media::mojom::VideoCaptureFormat::New();

  requested_settings->requested_format->frame_rate = capture_format.frame_rate;

  requested_settings->requested_format->pixel_format =
      GetVideoCapturePixelFormatFromPixelFormat(capture_format.pixel_format);

  requested_settings->requested_format->frame_size = gfx::mojom::Size::New();
  requested_settings->requested_format->frame_size->width =
      capture_format.frame_width;
  requested_settings->requested_format->frame_size->height =
      capture_format.frame_height;

  requested_settings->buffer_type =
      media::mojom::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor;
  active_device_->Start(std::move(requested_settings),
                        receiver_impl_.CreateInterfacePtr());
}

void MojoConnector::StopVideoCapture() {
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoConnector::StopVideoCaptureOnIpcThread,
                            base::Unretained(this)));
}

void MojoConnector::StopVideoCaptureOnIpcThread() {
  active_device_ = video_capture::mojom::DevicePtr();
}

void MojoConnector::CreateVirtualDevice(
    const VideoDevice& video_device,
    std::shared_ptr<ProducerImpl> producer_impl,
    const VideoCaptureServiceClient::VirtualDeviceCallback& callback) {
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoConnector::CreateVirtualDeviceOnIpcThread,
                            base::Unretained(this), video_device, producer_impl,
                            callback));
}

void MojoConnector::CreateVirtualDeviceOnIpcThread(
    const VideoDevice& video_device,
    std::shared_ptr<ProducerImpl> producer_impl,
    const VideoCaptureServiceClient::VirtualDeviceCallback& callback) {
  media::mojom::VideoCaptureDeviceInfoPtr info =
      media::mojom::VideoCaptureDeviceInfo::New();
  info->supported_formats =
      mojo::Array<media::mojom::VideoCaptureFormatPtr>::New(0);
  info->descriptor = media::mojom::VideoCaptureDeviceDescriptor::New();
  info->descriptor->model_id = video_device.model_id;
  info->descriptor->device_id = video_device.id;
  info->descriptor->display_name = video_device.display_name;
  info->descriptor->capture_api = media::mojom::VideoCaptureApi::VIRTUAL_DEVICE;
  producer_impl->RegisterVirtualDeviceAtFactory(&device_factory_,
                                                std::move(info));
  callback(video_device);
}

void MojoConnector::PushFrameToVirtualDevice(
    std::shared_ptr<ProducerImpl> producer_impl, base::TimeDelta timestamp,
    std::unique_ptr<const uint8_t[]> data, int data_size,
    PixelFormat pixel_format, int frame_width, int frame_height) {
  ipc_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&MojoConnector::PushFrameToVirtualDeviceOnIpcThread,
                            base::Unretained(this), producer_impl, timestamp,
                            base::Passed(&data), data_size, pixel_format,
                            frame_width, frame_height));
}

void MojoConnector::PushFrameToVirtualDeviceOnIpcThread(
    std::shared_ptr<ProducerImpl> producer_impl, base::TimeDelta timestamp,
    std::unique_ptr<const uint8_t[]> data, int data_size,
    PixelFormat pixel_format, int frame_width, int frame_height) {
  producer_impl->PushNextFrame(
      producer_impl, timestamp, std::move(data), data_size,
      GetVideoCapturePixelFormatFromPixelFormat(pixel_format), frame_width,
      frame_height);
}

void MojoConnector::OnShutdownComplete() { mojo::edk::ShutdownIPCSupport(); }

}  // namespace mri
