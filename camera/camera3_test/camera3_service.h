// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CAMERA3_TEST_CAMERA3_SERVICE_H_
#define CAMERA3_TEST_CAMERA3_SERVICE_H_

#include <semaphore.h>

#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "camera3_test/camera3_device_fixture.h"

namespace camera3_test {

const int32_t kNumberOfOutputStreamBuffers = 3;
const int32_t kPreviewOutputStreamIdx = 0;
const int32_t kRecordingOutputStreamIdx = 1;
// The still capture output stream can be at index 1 or 2, depending on whether
// there is video recording.
const int32_t kWaitForStopPreviewTimeoutMs = 3000;
const int32_t kWaitForFocusDoneTimeoutMs = 6000;
const int32_t kWaitForAWBConvergedTimeoutMs = 3000;
const int32_t kWaitForStopRecordingTimeoutMs = 3000;
enum { PREVIEW_STOPPED, PREVIEW_STARTING, PREVIEW_STARTED, PREVIEW_STOPPING };
#define INCREASE_INDEX(idx) \
  (idx) = (idx == number_of_capture_requests_ - 1) ? 0 : (idx) + 1

struct MetadataKeyValue {
  int32_t key;
  const void* data;
  size_t data_count;
  MetadataKeyValue(int32_t k, const void* d, size_t c)
      : key(k), data(d), data_count(c) {}
};

class Camera3Service {
 public:
  explicit Camera3Service(std::vector<int> cam_ids)
      : cam_ids_(cam_ids), initialized_(false) {}

  ~Camera3Service();

  typedef base::Callback<void(int cam_id,
                              uint32_t frame_number,
                              CameraMetadataUniquePtr metadata,
                              BufferHandleUniquePtr buffer)>
      ProcessStillCaptureResultCallback;

  typedef base::Callback<
      void(int cam_id, uint32_t frame_number, CameraMetadataUniquePtr metadata)>
      ProcessRecordingResultCallback;

  // Initialize service and corresponding devices and register processing
  // still capture and recording result callback
  int Initialize(ProcessStillCaptureResultCallback still_capture_cb,
                 ProcessRecordingResultCallback recording_cb);

  // Destroy service and corresponding devices
  void Destroy();

  // Start camera preview with given preview resolution |preview_resolution|
  // Set the width of |still_capture_resolution| or |recording_resolution| to 0
  // if taking still pictures or recording is not needed.
  int StartPreview(int cam_id,
                   const ResolutionInfo& preview_resolution,
                   const ResolutionInfo& still_capture_resolution,
                   const ResolutionInfo& recording_resolution);

  // Stop camera preview
  void StopPreview(int cam_id);

  // Start auto focus
  void StartAutoFocus(int cam_id);

  // Wait for auto focus done
  int WaitForAutoFocusDone(int cam_id);

  // Wait for AWB converged and lock AWB
  int WaitForAWBConvergedAndLock(int cam_id);

  // Start AE precapture
  void StartAEPrecapture(int cam_id);

  // Wait for AE stable
  int WaitForAEStable(int cam_id);

  // Take still capture with settings |metadata|
  void TakeStillCapture(int cam_id, const camera_metadata_t* metadata);

  // Start recording
  int StartRecording(int cam_id, const camera_metadata_t* metadata);

  // Stop recording
  void StopRecording(int cam_id);

  // Wait for |num_frames| number of preview frames with |timeout_ms|
  // milliseconds of timeout for each frame.
  int WaitForPreviewFrames(int cam_id,
                           uint32_t num_frames,
                           uint32_t timeout_ms);

  // Get device static information
  const Camera3Device::StaticInfo* GetStaticInfo(int cam_id);

  // Get device default request settings
  const camera_metadata_t* ConstructDefaultRequestSettings(int cam_id,
                                                           int type);

 private:
  std::vector<int> cam_ids_;

  base::Lock lock_;

  bool initialized_;

  class Camera3DeviceService;
  std::unordered_map<int, std::unique_ptr<Camera3DeviceService>>
      cam_dev_service_map_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Camera3Service);
};

class Camera3Service::Camera3DeviceService {
 public:
  Camera3DeviceService(int cam_id,
                       ProcessStillCaptureResultCallback still_capture_cb,
                       ProcessRecordingResultCallback recording_cb)
      : cam_id_(cam_id),
        cam_device_(cam_id),
        service_thread_("Camera3 Test Service Thread"),
        process_still_capture_result_cb_(still_capture_cb),
        process_recording_result_cb_(recording_cb),
        preview_state_(PREVIEW_STOPPED),
        number_of_capture_requests_(0),
        capture_request_idx_(0),
        number_of_in_flight_requests_(0),
        still_capture_metadata_(nullptr),
        recording_metadata_(nullptr) {}

  int Initialize();

  void Destroy();

  // Start camera preview with given preview resolution |preview_resolution|
  // Set the width of |still_capture_resolution| or |recording_resolution| to 0
  // if taking still pictures or recording is not needed.
  int StartPreview(const ResolutionInfo& preview_resolution,
                   const ResolutionInfo& still_capture_resolution,
                   const ResolutionInfo& recording_resolution);

  // Stop camera preview
  void StopPreview();

  // Start auto focus
  void StartAutoFocus();

  // Wait for auto focus done
  int WaitForAutoFocusDone();

  // Wait for AWB converged and lock AWB
  int WaitForAWBConvergedAndLock();

  // Start AE precapture
  void StartAEPrecapture();

  // Wait for AE stable
  int WaitForAEStable();

  // Take still capture with settings |metadata|
  void TakeStillCapture(const camera_metadata_t* metadata);

  // Start recording
  int StartRecording(const camera_metadata_t* metadata);

  // Stop recording
  void StopRecording();

  // Wait for |num_frames| number of preview frames with |timeout_ms|
  // milliseconds of timeout for each frame.
  int WaitForPreviewFrames(uint32_t num_frames, uint32_t timeout_ms);

  // Get static information
  const Camera3Device::StaticInfo* GetStaticInfo() const;

  // Get default request settings
  const camera_metadata_t* ConstructDefaultRequestSettings(int type);

 private:
  // Process result metadata and output buffers
  void ProcessResultMetadataOutputBuffers(
      uint32_t frame_number,
      CameraMetadataUniquePtr metadata,
      std::vector<BufferHandleUniquePtr> buffers);

  void StartPreviewOnServiceThread(ResolutionInfo preview_resolution,
                                   ResolutionInfo still_capture_resolution,
                                   ResolutionInfo recording_resolution,
                                   int* result);

  void StartAutoFocusOnServiceThread();

  void StopPreviewOnServiceThread(base::Callback<void()> cb);

  void AddMetadataListenerOnServiceThread(int32_t key,
                                          std::unordered_set<int32_t> values,
                                          base::Callback<void()> cb,
                                          int32_t* result);

  void DeleteMetadataListenerOnServiceThread(
      int32_t key,
      std::unordered_set<int32_t> values);

  void LockAWBOnServiceThread();

  void StartAEPrecaptureOnServiceThread();

  void TakeStillCaptureOnServiceThread(const camera_metadata_t* metadata,
                                       base::Callback<void()> cb);

  void StartRecordingOnServiceThread(const camera_metadata_t* metadata,
                                     base::Callback<void(int)> cb);

  void StopRecordingOnServiceThread(base::Callback<void()> cb);

  // This function can be called by PrepareStillCaptureAndStartPreview() or
  // ProcessResultMetadataOutputBuffers() to process one preview request.
  // It will check whether there was a still capture request or preview
  // repeating/one-shot setting changes and construct the capture request
  // accordingly.
  void ProcessPreviewRequestOnServiceThread();

  void ProcessResultMetadataOutputBuffersOnServiceThread(
      uint32_t frame_number,
      CameraMetadataUniquePtr metadata,
      std::vector<BufferHandleUniquePtr> buffers);

  int cam_id_;

  Camera3Device cam_device_;

  cros::CameraThread service_thread_;

  ProcessStillCaptureResultCallback process_still_capture_result_cb_;

  ProcessRecordingResultCallback process_recording_result_cb_;

  int32_t preview_state_;

  base::Callback<void()> stop_preview_cb_;

  std::vector<const camera3_stream_t*> streams_;

  uint32_t number_of_capture_requests_;

  // Keep |number_of_capture_requests_| number of capture request
  std::vector<camera3_capture_request_t> capture_requests_;

  // Keep track of two stream buffers for each capture request. The preview
  // buffer is at index 0 while still capture one at index 1.
  std::vector<std::vector<camera3_stream_buffer_t>> output_stream_buffers_;

  // The index of capture request that is going to have its corresponding
  // capture result returned
  size_t capture_request_idx_;

  // Number of capture requests that are being processed by HAL
  size_t number_of_in_flight_requests_;

  // Metadata for repeating preview requests
  CameraMetadataUniquePtr repeating_preview_metadata_;

  // Metadata for one-shot preview requests. It can be used to trigger AE
  // precapture and auto focus.
  CameraMetadataUniquePtr oneshot_preview_metadata_;

  // Metadata for still capture requests
  const camera_metadata_t* still_capture_metadata_;

  base::Callback<void()> still_capture_cb_;

  // Metadata for recording requests
  const camera_metadata_t* recording_metadata_;

  base::Callback<void()> stop_recording_cb_;

  struct MetadataListener {
    int32_t key;
    std::unordered_set<int32_t> values;
    base::Callback<void()> cb;
    int32_t* result;
    MetadataListener(int32_t k,
                     const std::unordered_set<int32_t>& v,
                     base::Callback<void()> c,
                     int32_t* r)
        : key(k), values(v), cb(c), result(r) {}
  };

  std::list<MetadataListener> metadata_listener_list_;

  sem_t preview_frame_sem_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Camera3DeviceService);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_SERVICE_H_
