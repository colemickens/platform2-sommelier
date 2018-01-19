/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_USB_V1_ARC_CAMERA_SERVICE_H_
#define HAL_USB_V1_ARC_CAMERA_SERVICE_H_

#include <memory>
#include <string>

#include <base/threading/thread.h>
#include <base/threading/thread_checker.h>
#include <mojo/edk/embedder/process_delegate.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "hal/usb_v1/arc_camera.mojom.h"
#include "hal/usb_v1/camera_device_delegate.h"

namespace arc {

class ArcCameraServiceImpl : public ArcCameraService,
                             public mojo::edk::ProcessDelegate {
 public:
  ArcCameraServiceImpl(int socket_fd, base::Closure quit_cb);
  ~ArcCameraServiceImpl();

  // Create a mojo connection to container.
  bool Start();

  // ProcessDelegate implementation.
  void OnShutdownComplete() override {}

 private:
  void OnChannelClosed(const std::string& error_msg);

  // ArcCameraService:
  void Connect(const mojo::String& device_path,
               const ConnectCallback& callback) override;
  void Disconnect(const DisconnectCallback& callback) override;
  void StreamOn(uint32_t width,
                uint32_t height,
                uint32_t pixel_format,
                float frame_rate,
                const StreamOnCallback& callback) override;
  void StreamOff(const StreamOffCallback& callback) override;
  void GetNextFrameBuffer(const GetNextFrameBufferCallback& callback) override;
  void ReuseFrameBuffer(uint32_t buffer_id,
                        const ReuseFrameBufferCallback& callback) override;
  void GetDeviceSupportedFormats(
      const mojo::String& device_path,
      const GetDeviceSupportedFormatsCallback& callback) override;
  void GetCameraDeviceInfos(
      const GetCameraDeviceInfosCallback& callback) override;

  base::ScopedFD socket_fd_;

  // Quit callback to exit daemon.
  base::Closure quit_cb_;

  // Mojo endpoints.
  mojo::Binding<ArcCameraService> binding_;

  // Real camera device.
  std::unique_ptr<CameraDeviceDelegate> camera_device_;

  // Thread used in mojo to send and receive IPC messages.
  base::Thread ipc_thread_;

  DISALLOW_COPY_AND_ASSIGN(ArcCameraServiceImpl);
};

}  // namespace arc

#endif  // HAL_USB_V1_ARC_CAMERA_SERVICE_H_
