/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HAL_ADAPTER_CAMERA_DEVICE_ADAPTER_H_
#define HAL_ADAPTER_CAMERA_DEVICE_ADAPTER_H_

#include <memory>
#include <unordered_map>

#include <hardware/camera3.h>

#include <base/files/scoped_file.h>
#include <base/synchronization/lock.h>
#include <base/threading/thread.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "arc/camera_buffer_mapper.h"
#include "hal_adapter/arc_camera3_mojo_utils.h"
#include "hal_adapter/common_types.h"
#include "hal_adapter/mojo/arc_camera3.mojom.h"

namespace arc {

class Camera3DeviceOpsDelegate;

class Camera3CallbackOpsDelegate;

class CameraDeviceAdapter : public camera3_callback_ops_t {
 public:
  explicit CameraDeviceAdapter(camera3_device_t* camera_device,
                               base::Callback<void()> close_callback);

  ~CameraDeviceAdapter();

  // Starts the camera device adapter.  This method must be called before all
  // the other methods are called.
  bool Start();

  // Bind() is called by CameraHalAdapter in OpenDevice() on the mojo IPC
  // handler thread in |module_delegate_|.
  void Bind(mojom::Camera3DeviceOpsRequest device_ops_request);

  // Callback interface for Camera3DeviceOpsDelegate.
  // These methods are callbacks for |device_ops_delegate_| and are executed on
  // the mojo IPC handler thread in |device_ops_delegate_|.

  int32_t Initialize(mojom::Camera3CallbackOpsPtr callback_ops);

  int32_t ConfigureStreams(
      mojom::Camera3StreamConfigurationPtr config,
      mojom::Camera3StreamConfigurationPtr* updated_config);

  mojom::CameraMetadataPtr ConstructDefaultRequestSettings(
      mojom::Camera3RequestTemplate type);

  int32_t ProcessCaptureRequest(mojom::Camera3CaptureRequestPtr request);

  void Dump(mojo::ScopedHandle fd);

  int32_t Flush();

  int32_t RegisterBuffer(uint64_t buffer_id,
                         mojom::Camera3DeviceOps::BufferType type,
                         mojo::Array<mojo::ScopedHandle> fds,
                         uint32_t drm_format,
                         mojom::HalPixelFormat hal_pixel_format,
                         uint32_t width,
                         uint32_t height,
                         mojo::Array<uint32_t> strides,
                         mojo::Array<uint32_t> offsets);

  int32_t Close();

 private:
  // Implementation of camera3_callback_ops_t.
  static void ProcessCaptureResult(const camera3_callback_ops_t* ops,
                                   const camera3_capture_result_t* result);

  static void Notify(const camera3_callback_ops_t* ops,
                     const camera3_notify_msg_t* msg);

  // NOTE: All the fds in |result| (e.g. fences and buffer handles) will be
  // closed after the function returns.  The caller needs to dup a fd in
  // |result| if the fd will be accessed after calling ProcessCaptureResult.
  mojom::Camera3CaptureResultPtr PrepareCaptureResult(
      const camera3_capture_result_t* result);

  mojom::Camera3NotifyMsgPtr PrepareNotifyMsg(const camera3_notify_msg_t* msg);

  // Caller must hold |buffer_handles_lock_|.
  void RemoveBufferLocked(const camera3_stream_buffer_t& buffer);

  // Waits until |release_fence| is signaled and then deletes |buffer|.
  void RemoveBufferOnFenceSyncThread(
      base::ScopedFD release_fence,
      std::unique_ptr<camera_buffer_handle_t> buffer);

  void ResetDeviceOpsDelegateOnThread();
  void ResetCallbackOpsDelegateOnThread();

  // The thread that all the camera3 device ops operate on.
  base::Thread camera_device_ops_thread_;

  // The thread that all the Mojo communications of camera3 callback ops operate
  // on.
  base::Thread camera_callback_ops_thread_;

  // A thread to asynchronously wait for release fences and destroy
  // corresponding buffer handles.
  base::Thread fence_sync_thread_;

  // The delegate that handles the Camera3DeviceOps mojo IPC.
  std::unique_ptr<Camera3DeviceOpsDelegate> device_ops_delegate_;

  // The delegate that handles the Camera3CallbackOps mojo IPC.
  std::unique_ptr<Camera3CallbackOpsDelegate> callback_ops_delegate_;

  // The callback to run when the device is closed.
  base::Callback<void()> close_callback_;

  // The real camera device.
  camera3_device_t* camera_device_;

  // A mapping from Andoird HAL for all the configured streams.
  internal::UniqueStreams streams_;

  // A mutex to guard |streams_|.
  base::Lock streams_lock_;

  // A mapping from the locally created buffer handle to the Android HAL handle
  // ID.  We need to return the correct handle ID in ProcessCaptureResult so ARC
  // can restore the buffer handle in the capture result before passing up to
  // the frameworks.
  std::unordered_map<uint64_t, std::unique_ptr<camera_buffer_handle_t>>
      buffer_handles_;

  // A mutex to guard |buffer_handles_|.
  base::Lock buffer_handles_lock_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CameraDeviceAdapter);
};

}  // namespace arc

#endif  // HAL_ADAPTER_CAMERA_DEVICE_ADAPTER_H_
