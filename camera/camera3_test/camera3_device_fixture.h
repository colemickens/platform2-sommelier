// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_DEVICE_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_DEVICE_FIXTURE_H_

#include <memory>
#include <unordered_map>

#include <base/synchronization/lock.h>
#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include <hardware/hardware.h>

#include "camera3_module_fixture.h"
#include "camera3_test_gralloc.h"
#include "common/camera_buffer_handle.h"

namespace camera3_test {

class Camera3Device {
 public:
  explicit Camera3Device(int cam_id)
      : cam_id_(cam_id),
        initialized_(false),
        cam_device_(NULL),
        hal_thread_(GetThreadName(cam_id).c_str()),
        cam_stream_idx_(0),
        gralloc_(Camera3TestGralloc::GetInstance()) {}

  ~Camera3Device() {}

  // Initialize
  int Initialize(Camera3Module* cam_module,
                 const camera3_callback_ops_t* callback_ops);

  // Destroy
  void Destroy();

  // Whether or not the template is supported
  bool IsTemplateSupported(int32_t type) const;

  // Construct default request settings
  const camera_metadata_t* ConstructDefaultRequestSettings(int type);

  // Add output stream in preparation for stream configuration
  void AddOutputStream(int format, int width, int height);

  // Configure streams
  int ConfigureStreams();

  // Allocate output buffers
  int AllocateOutputStreamBuffers(
      std::vector<camera3_stream_buffer_t>* output_buffers);

  // Allocate output buffers by given streams. This function is designed
  // specifically for testing exceptional cases like unconfigured streams. Don't
  // use it otherwise. However, if |streams| is empty, this function will use
  // the streams configured by |AddOutputStream| and |ConfigureStreams|; i.e. it
  // will behave exacly the same as |AllocateOutputStreamBuffers| does.
  int AllocateOutputStreamBuffersByStreams(
      const std::vector<camera3_stream_t>& streams,
      std::vector<camera3_stream_buffer_t>* output_buffers);

  // Free stream buffers
  int FreeOutputStreamBuffers(
      const std::vector<camera3_stream_buffer_t>& output_buffers);

  // Free all stream buffers
  void ClearOutputStreamBuffers();

  // Process capture request
  int ProcessCaptureRequest(camera3_capture_request_t* request);

  // Flush all currently in-process captures and all buffers in the pipeline
  int Flush();

  // Get static information
  class StaticInfo;
  const StaticInfo* GetStaticInfo() const;

 private:
  const std::string GetThreadName(int cam_id);

  void InitializeOnHalThread(const camera3_callback_ops_t* callback_ops,
                             int* result);

  void ConstructDefaultRequestSettingsOnHalThread(
      int type,
      const camera_metadata_t** result);

  void ConfigureStreamsOnHalThread(camera3_stream_configuration_t* config,
                                   int* result);

  void ProcessCaptureRequestOnHalThread(camera3_capture_request_t* request,
                                        int* result);

  void FlushOnHalThread(int* result);

  void CloseOnHalThread(int* result);

  const int cam_id_;

  bool initialized_;

  camera3_device* cam_device_;

  std::unique_ptr<StaticInfo> static_info_;

  // This thread is needed because of the ARC++ HAL assumption that all the
  // camera3_device_ops functions, except dump, should be called on the same
  // thread. Each device is accessed through a different thread.
  Camera3TestThread hal_thread_;

  // Two bins of streams for swapping while configuring new streams
  std::vector<camera3_stream_t> cam_stream_[2];

  // Index of active streams
  int cam_stream_idx_;

  Camera3TestGralloc* gralloc_;

  // Store allocated buffers while easy to lookup and remove
  std::unordered_map<camera3_stream_t*, std::set<BufferHandleUniquePtr>>
      stream_buffers_;

  base::Lock stream_lock_;

  base::Lock stream_buffers_lock_;

  DISALLOW_COPY_AND_ASSIGN(Camera3Device);
};

class Camera3Device::StaticInfo {
 public:
  StaticInfo(const camera_info& cam_info);

  // Determine whether or not all the keys are available
  bool IsKeyAvailable(uint32_t tag) const;
  bool AreKeysAvailable(std::vector<uint32_t> tags) const;

  // Whether or not the hardware level reported is at least full
  bool IsHardwareLevelAtLeastFull() const;

  // Whether or not the hardware level reported is at least limited
  bool IsHardwareLevelAtLeastLimited() const;

  // Determine whether the current device supports a capability or not
  bool IsCapabilitySupported(int capability) const;

  // Check if depth output is supported, based on the depth capability
  bool IsDepthOutputSupported() const;

  // Check if standard outputs (PRIVATE, YUV, JPEG) outputs are supported,
  // based on the backwards-compatible capability
  bool IsColorOutputSupported() const;

  // Get available edge modes
  std::set<uint8_t> GetAvailableEdgeModes() const;

  // Get available noise reduction modes
  std::set<uint8_t> GetAvailableNoiseReductionModes() const;

  // Get available noise reduction modes
  std::set<uint8_t> GetAvailableColorAberrationModes() const;

  // Get available tone map modes
  std::set<uint8_t> GetAvailableToneMapModes() const;

  // Get available formats for a given direction
  // direction: ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT or
  //            ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT
  std::set<int32_t> GetAvailableFormats(int32_t direction) const;

  // Check if a stream format is supported
  bool IsFormatAvailable(int format) const;

  // Get the image output resolutions in this stream configuration
  std::vector<ResolutionInfo> GetSortedOutputResolutions(int32_t format) const;

  // Determine if camera device support AE lock control
  bool IsAELockSupported() const;

  // Determine if camera device support AWB lock control
  bool IsAWBLockSupported() const;

  // Get the maximum number of partial result a request can expect
  int32_t GetPartialResultCount() const;

  // Get the maxium size of JPEG image
  int32_t GetJpegMaxSize() const;

  // Get the sensor orientation
  int32_t GetSensorOrientation() const;

 private:
  // Return the supported hardware level of the device, or fail if no value is
  // reported
  int32_t GetHardwareLevel() const;

  bool IsHardwareLevelAtLeast(int32_t level) const;

  std::set<uint8_t> GetAvailableModes(int32_t key,
                                      int32_t min_value,
                                      int32_t max_value) const;

  void GetStreamConfigEntry(camera_metadata_ro_entry_t* entry) const;

  const camera_metadata_t* characteristics_;
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
