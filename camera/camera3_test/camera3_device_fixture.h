// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_DEVICE_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_DEVICE_FIXTURE_H_

#include <memory>
#include <unordered_map>

#include <base/synchronization/lock.h>
#include <gbm.h>
#include <gralloc_drm_handle.h>
#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include <hardware/hardware.h>
#include <xf86drm.h>

#include "camera3_module_fixture.h"

namespace camera3_test {

class Camera3TestGralloc {
 public:
  int Initialize();

  void Destroy();

  int Allocate(int width,
               int height,
               int format,
               int usage,
               buffer_handle_t* handle,
               int* stride);

  int Free(buffer_handle_t handle);

 private:
  struct gbm_device* CreateGbmDevice();

  // Conversion from HAL to GBM usage flags
  uint64_t GrallocConvertFlags(int flags);

  // Conversion from HAL to fourcc-based GBM formats
  uint32_t GrallocConvertFormat(int format);

  gbm_device* gbm_dev_;
};

class Camera3Device {
 public:
  explicit Camera3Device(int cam_id)
      : cam_id_(cam_id),
        initialized_(false),
        cam_device_(NULL),
        cam_stream_idx_(0) {}

  ~Camera3Device() {}

  // Initialize
  int Initialize(const Camera3Module* cam_module,
                 const camera3_callback_ops_t* callback_ops);

  // Destroy
  void Destroy();

  // Construct default request settings
  const camera_metadata_t* ConstructDefaultRequestSettings(int type);

  // Add output stream in preparation for stream configuration
  void AddOutputStream(int format, int width, int height);

  // Configure streams
  int ConfigureStreams();

  // Allocate stream buffers
  int AllocateOutputStreamBuffers(
      std::vector<camera3_stream_buffer_t>* output_buffers);

  // Free stream buffers
  int FreeOutputStreamBuffers(
      const std::vector<camera3_stream_buffer_t>& output_buffers);

  // Process capture request
  int ProcessCaptureRequest(camera3_capture_request_t* request);

 private:
  const int cam_id_;

  bool initialized_;

  camera3_device* cam_device_;

  // Two bins of streams for swapping while configuring new streams
  std::vector<camera3_stream_t> cam_stream_[2];

  // Index of active streams
  int cam_stream_idx_;

  Camera3TestGralloc gralloc_;

  // Store allocated buffers while easy to lookup and remove
  std::unordered_map<camera3_stream_t*,
                     std::set<std::unique_ptr<buffer_handle_t>>>
      stream_buffers_;

  base::Lock stream_lock_;

  base::Lock stream_buffers_lock_;

  DISALLOW_COPY_AND_ASSIGN(Camera3Device);
};

class Camera3DeviceFixture : public testing::Test,
                             protected camera3_callback_ops {
 public:
  explicit Camera3DeviceFixture(int cam_id) : cam_device_(cam_id) {}

  virtual void SetUp() override;

  virtual void TearDown() override;

 protected:
  // Callback functions to process capture result
  virtual void ProcessCaptureResult(const camera3_capture_result* result);

  // Callback functions to handle notifications
  virtual void Notify(const camera3_notify_msg* msg);

  Camera3Module cam_module_;

  Camera3Device cam_device_;

 private:
  // Static callback forwarding methods from HAL to instance
  static void ProcessCaptureResultCallback(
      const camera3_callback_ops* cb,
      const camera3_capture_result* result);

  // Static callback forwarding methods from HAL to instance
  static void NotifyCallback(const camera3_callback_ops* cb,
                             const camera3_notify_msg* msg);

  DISALLOW_COPY_AND_ASSIGN(Camera3DeviceFixture);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_DEVICE_FIXTURE_H_
