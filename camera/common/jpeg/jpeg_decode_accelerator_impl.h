/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CAMERA_COMMON_JPEG_JPEG_DECODE_ACCELERATOR_IMPL_H_
#define CAMERA_COMMON_JPEG_JPEG_DECODE_ACCELERATOR_IMPL_H_

#include <stdint.h>
#include <memory>
#include <set>
#include <unordered_map>

#include <base/memory/shared_memory.h>
#include <base/threading/thread.h>

#include "cros-camera/camera_metrics.h"
#include "cros-camera/camera_mojo_channel_manager.h"
#include "cros-camera/future.h"
#include "cros-camera/jpeg_decode_accelerator.h"
#include "mojo/cros_camera_service.mojom.h"

namespace cros {

namespace tests {
class JpegDecodeAcceleratorTest;
}  // namespace tests

// Encapsulates a JPEG decoder. This class is not thread-safe.
// Before using this class, make sure mojo is initialized first.
class JpegDecodeAcceleratorImpl final : public JpegDecodeAccelerator {
 public:
  JpegDecodeAcceleratorImpl();

  ~JpegDecodeAcceleratorImpl() final;

  // JpegDecodeAccelerator implementation.

  bool Start() final;

  JpegDecodeAccelerator::Error DecodeSync(int input_fd,
                                          uint32_t input_buffer_size,
                                          uint32_t input_buffer_offset,
                                          buffer_handle_t output_buffer) final;

  JpegDecodeAccelerator::Error DecodeSync(int input_fd,
                                          uint32_t input_buffer_size,
                                          int32_t coded_size_width,
                                          int32_t coded_size_height,
                                          int output_fd,
                                          uint32_t output_buffer_size) final;

  int32_t Decode(int input_fd,
                 uint32_t input_buffer_size,
                 uint32_t input_buffer_offset,
                 buffer_handle_t output_buffer,
                 DecodeCallback callback) final;

  int32_t Decode(int input_fd,
                 uint32_t input_buffer_size,
                 int32_t coded_size_width,
                 int32_t coded_size_height,
                 int output_fd,
                 uint32_t output_buffer_size,
                 DecodeCallback callback) final;

 private:
  // To let test class access private testing methods.
  // e.g. ResetJDAChannel()
  friend class tests::JpegDecodeAcceleratorTest;

  // Map from buffer ID to input shared memory.
  using InputShmMap =
      std::unordered_map<int32_t, std::unique_ptr<base::SharedMemory>>;

  // Initialize Mojo channel to GPU pcorss in chrome.
  // And initialize the JpegDecodeAccelerator.
  void InitializeOnIpcThread(base::Callback<void(bool)> callback);

  // Destroy resources on IPC thread.
  void DestroyOnIpcThread();

  // Error handler for JDA mojo channel.
  void OnJpegDecodeAcceleratorError();

  // Process decode request on IPC thread with output buffer_handle_t.
  void DecodeOnIpcThread(int32_t buffer_id,
                         int input_fd,
                         uint32_t input_buffer_size,
                         uint32_t input_buffer_offset,
                         buffer_handle_t output_buffer,
                         DecodeCallback callback);

  // Process decode request on IPC thread with output shm fd.
  void DecodeOnIpcThreadLegacy(int32_t buffer_id,
                               int input_fd,
                               uint32_t input_buffer_size,
                               int32_t coded_size_width,
                               int32_t coded_size_height,
                               int output_fd,
                               uint32_t output_buffer_size,
                               DecodeCallback callback);

  // Callback function for |jda_ptr_|->DecodeWithDmaBuf().
  void OnDecodeAck(DecodeCallback callback,
                   int32_t buffer_id,
                   cros::mojom::DecodeError error);

  // Callback function for |jda_ptr_|->DecodeWithFD().
  void OnDecodeAckLegacy(DecodeCallback callback,
                         int32_t buffer_id,
                         cros::mojom::DecodeError error);

  // For synced Decode API.
  void DecodeSyncCallback(base::Callback<void(int)> callback,
                          int32_t buffer_id,
                          int error);

  // Reset JDA Mojo channel. It is used for testing.
  void TestResetJDAChannel();
  void TestResetJDAChannelOnIpcThread(scoped_refptr<cros::Future<void>> future);

  // Camera Mojo channel manager.
  // We use it to create JpegDecodeAccelerator Mojo channel.
  std::unique_ptr<CameraMojoChannelManager> mojo_channel_manager_;

  // Used to cancel pending futures when error occurs.
  std::unique_ptr<cros::CancellationRelay> cancellation_relay_;

  // Pointer to local proxy of remote JpegDecodeAccelerator interface
  // implementation.
  // All the Mojo communication to |jda_ptr_| happens on |ipc_thread_|.
  mojom::MjpegDecodeAcceleratorPtr jda_ptr_;

  // Thread for IPC chores.
  base::Thread ipc_thread_;

  // The id for current buffer being decoded.
  int32_t buffer_id_;

  // Tracking the buffer ids sent to decoder.
  std::set<int32_t> inflight_buffer_ids_;

  // A map from buffer id to input shared memory.
  // |input_shm_map_| should only be accessed on ipc_thread_.
  // The input shared memory is used to store JPEG stream buffer.
  // Since the input buffer may be from DMA buffer, we need to prepare a shared
  // memory for JpegDecodeAccelerator interface.
  // We will send the handle of the shared memory to the remote process, so we
  // need to keep the lifecycle of the shared memory until we receive DecodeAck.
  InputShmMap input_shm_map_;

  // Metrics that used to record things like decoding latency.
  std::unique_ptr<CameraMetrics> camera_metrics_;

  DISALLOW_COPY_AND_ASSIGN(JpegDecodeAcceleratorImpl);
};

}  // namespace cros
#endif  // CAMERA_COMMON_JPEG_JPEG_DECODE_ACCELERATOR_IMPL_H_
