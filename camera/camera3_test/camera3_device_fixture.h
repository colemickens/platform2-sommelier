// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_DEVICE_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_DEVICE_FIXTURE_H_

#include <semaphore.h>

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

struct CameraMetadataDeleter {
  inline void operator()(camera_metadata_t* metadata) {
    if (metadata) {
      free_camera_metadata(metadata);
    }
  }
};

typedef std::unique_ptr<camera_metadata_t, struct CameraMetadataDeleter>
    CameraMetadataUniquePtr;

const uint32_t kInitialFrameNumber = 0;

class Camera3Device : protected camera3_callback_ops {
 public:
  explicit Camera3Device(int cam_id)
      : cam_id_(cam_id),
        initialized_(false),
        cam_device_(NULL),
        hal_thread_(GetThreadName(cam_id).c_str()),
        cam_stream_idx_(0),
        gralloc_(Camera3TestGralloc::GetInstance()),
        request_frame_number_(kInitialFrameNumber),
        result_frame_number_(kInitialFrameNumber) {}

  ~Camera3Device() {}

  // Initialize
  int Initialize(Camera3Module* cam_module);

  // Destroy
  void Destroy();

  typedef base::Callback<void(const camera3_capture_result* result)>
      ProcessCaptureResultCallback;

  typedef base::Callback<void(const camera3_notify_msg* msg)> NotifyCallback;

  typedef base::Callback<void(uint32_t frame_number,
                              CameraMetadataUniquePtr metadata,
                              std::vector<BufferHandleUniquePtr>* buffers)>
      ProcessResultMetadataOutputBuffersCallback;

  typedef base::Callback<void(
      std::vector<CameraMetadataUniquePtr>* partial_metadata)>
      ProcessPartialMetadataCallback;

  // Register callback function to process capture result
  void RegisterProcessCaptureResultCallback(ProcessCaptureResultCallback cb);

  // Register callback function for notification
  void RegisterNotifyCallback(NotifyCallback cb);

  // Register callback function to process result metadata and output buffers
  void RegisterResultMetadataOutputBufferCallback(
      ProcessResultMetadataOutputBuffersCallback cb);

  // Register callback function to process partial metadata
  void RegisterPartialMetadataCallback(ProcessPartialMetadataCallback cb);

  // Whether or not the template is supported
  bool IsTemplateSupported(int32_t type) const;

  // Construct default request settings
  const camera_metadata_t* ConstructDefaultRequestSettings(int type);

  // Add output stream in preparation for stream configuration
  void AddOutputStream(int format, int width, int height);

  // Configure streams and return configured streams if |streams| is not null
  int ConfigureStreams(std::vector<const camera3_stream_t*>* streams);

  // Allocate output buffers for all configured streams and return them
  // in the stream buffer format, which has the buffer associated to the
  // corresponding stream. The allocated buffers are owned by Camera3Device.
  int AllocateOutputStreamBuffers(
      std::vector<camera3_stream_buffer_t>* output_buffers);

  // Allocate output buffers for given streams |streams| and return them
  // in the stream buffer format, which has the buffer associated to the
  // corresponding stream. The allocated buffers are owned by Camera3Device.
  int AllocateOutputBuffersByStreams(
      const std::vector<const camera3_stream_t*>& streams,
      std::vector<camera3_stream_buffer_t>* output_buffers);

  // Get the buffers out of the given stream buffers |output_buffers|. The
  // buffers are return in the container |unique_buffers|, and the caller of
  // the function is expected to take the buffer ownership.
  int GetOutputStreamBufferHandles(
      const std::vector<camera3_stream_buffer_t>& output_buffers,
      std::vector<BufferHandleUniquePtr>* unique_buffers);

  // Register buffer |unique_buffer| that is associated with the given stream
  // |stream|. Camera3Device takes buffer ownership.
  int RegisterOutputBuffer(const camera3_stream_t& stream,
                           BufferHandleUniquePtr unique_buffer);

  // Process given capture request |capture_request|. The frame number field of
  // |capture_request| will be overwritten if this method returns 0 on success.
  int ProcessCaptureRequest(camera3_capture_request_t* capture_request);

  // Wait for shutter with timeout. |abs_timeout| specifies an absolute timeout
  // in seconds and nanoseconds since the Epoch, 1970-01-01 00:00:00 +0000
  // (UTC), that the call should block if the shutter is immediately available.
  int WaitShutter(const struct timespec& abs_timeout);

  // Wait for capture result with timeout. |abs_timeout| specifies an absolute
  // timeout in seconds and nanoseconds since the Epoch, 1970-01-01 00:00:00
  // +0000 (UTC), that the call should block if the shutter is immediately
  // available.
  int WaitCaptureResult(const struct timespec& abs_timeout);

  // Flush all currently in-process captures and all buffers in the pipeline
  int Flush();

  // Get static information
  class StaticInfo;
  const StaticInfo* GetStaticInfo() const;

 protected:
  // Callback functions from HAL device
  void ProcessCaptureResult(const camera3_capture_result* result);

  // Callback functions from HAL device
  void Notify(const camera3_notify_msg* msg);

 private:
  const std::string GetThreadName(int cam_id);

  // Static callback forwarding methods from HAL to instance
  static void ProcessCaptureResultForwarder(
      const camera3_callback_ops* cb,
      const camera3_capture_result* result);

  // Static callback forwarding methods from HAL to instance
  static void NotifyForwarder(const camera3_callback_ops* cb,
                              const camera3_notify_msg* msg);

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

  // Whether or not partial result is used
  bool UsePartialResult() const;

  // Process and handle partial result of one callback
  void ProcessPartialResult(const camera3_capture_result& result);

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

  // Store allocated buffers with streams as the key
  std::unordered_map<const camera3_stream_t*,
                     std::vector<BufferHandleUniquePtr>>
      stream_buffer_map_;

  // Lock to protect |cam_stream_|
  base::Lock stream_lock_;

  // Lock to protect |stream_buffer_map_|
  base::Lock stream_buffer_map_lock_;

  // Lock to protect |capture_request_map_| and |request_frame_number_|.
  base::Lock request_lock_;

  uint32_t request_frame_number_;

  uint32_t result_frame_number_;

  // Store created capture requests with frame number as the key
  std::unordered_map<uint32_t, camera3_capture_request_t> capture_request_map_;

  // Store the frame numbers of capture requests that HAL has finished
  // processing
  std::set<uint32_t> completed_request_set_;

  class CaptureResultInfo {
   public:
    CaptureResultInfo()
        : num_output_buffers_(0), have_result_metadata_(false) {}

    // Allocate and copy into partial metadata
    void AllocateAndCopyMetadata(const camera_metadata_t& src);

    // Determine whether or not the key is available
    bool IsMetadataKeyAvailable(int32_t key) const;

    // Find and get key value from partial metadata
    int32_t GetMetadataKeyValue(int32_t key) const;

    // Find and get key value in int64_t from partial metadata
    int64_t GetMetadataKeyValue64(int32_t key) const;

    // Merge partial metadata into one.
    CameraMetadataUniquePtr MergePartialMetadata();

    uint32_t num_output_buffers_;

    bool have_result_metadata_;

    std::vector<CameraMetadataUniquePtr> partial_metadata_;

    std::vector<camera3_stream_buffer_t> output_buffers_;

   private:
    bool GetMetadataKeyEntry(int32_t key,
                             camera_metadata_ro_entry_t* entry) const;
  };

  // Store capture result information with frame number as the key
  std::unordered_map<uint32_t, CaptureResultInfo> capture_result_info_map_;

  sem_t shutter_sem_;

  sem_t capture_result_sem_;

  ProcessCaptureResultCallback process_capture_result_cb_;

  NotifyCallback notify_cb_;

  ProcessResultMetadataOutputBuffersCallback
      process_result_metadata_output_buffers_cb_;

  ProcessPartialMetadataCallback process_partial_metadata_cb_;

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
  // Returns: maximum number of partial results; it is 1 by default.
  int32_t GetPartialResultCount() const;

  // Get the number of maximum pipeline stages a frame has to go through from
  // when it's exposed to when it's available to the framework
  // Returns: number of maximum pipeline stages on success; corresponding error
  // code on failure.
  int32_t GetRequestPipelineMaxDepth() const;

  // Get the maxium size of JPEG image
  // Returns: maximum size of JPEG image on success; corresponding error code
  // on failure.
  int32_t GetJpegMaxSize() const;

  // Get the sensor orientation
  // Returns: degrees on success; corresponding error code on failure.
  int32_t GetSensorOrientation() const;

  // Get available thumbnail sizes
  // Returns: 0 on success; corresponding error code on failure.
  int32_t GetAvailableThumbnailSizes(
      std::vector<ResolutionInfo>* resolutions) const;

  // Get available focal lengths
  // Returns: 0 on success; corresponding error code on failure.
  int32_t GetAvailableFocalLengths(std::vector<float>* focal_lengths) const;

  // Get available apertures
  // Returns: 0 on success; corresponding error code on failure.
  int32_t GetAvailableApertures(std::vector<float>* apertures) const;

  // Get available AF modes
  // Returns: 0 on success; corresponding error code on failure.
  int32_t GetAvailableAFModes(std::vector<int32_t>* af_modes) const;

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

class Camera3DeviceFixture : public testing::Test {
 public:
  explicit Camera3DeviceFixture(int cam_id) : cam_device_(cam_id) {}

  virtual void SetUp() override;

  virtual void TearDown() override;

 protected:

  Camera3Module cam_module_;

  Camera3Device cam_device_;

 private:
  // Process result metadata and/or output buffers. Tests can override this
  // function to handle metadata/buffers to suit their purpose. Note that
  // the metadata |metadata| and output buffers kept in |buffers| will be
  // freed after returning from this call; a test can "std::move" the unique
  // pointers to keep the metadata and buffer.
  virtual void ProcessResultMetadataOutputBuffers(
      uint32_t frame_number,
      CameraMetadataUniquePtr metadata,
      std::vector<BufferHandleUniquePtr>* buffers) {}

  // Process partial metadata. Tests can override this function to handle all
  // received partial metadata.
  virtual void ProcessPartialMetadata(
      std::vector<CameraMetadataUniquePtr>* partial_metadata) {}

  DISALLOW_COPY_AND_ASSIGN(Camera3DeviceFixture);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_DEVICE_FIXTURE_H_
