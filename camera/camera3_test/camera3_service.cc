// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_service.h"

#include <base/bind.h>
#include <unistd.h>

namespace camera3_test {

Camera3Service::~Camera3Service() {}

int Camera3Service::Initialize() {
  return Initialize(ProcessStillCaptureResultCallback());
}

int Camera3Service::Initialize(ProcessStillCaptureResultCallback cb) {
  base::AutoLock l(lock_);
  if (initialized_) {
    LOG(ERROR) << "Camera service is already initialized";
    return -EINVAL;
  }
  for (const auto& it : cam_ids_) {
    cam_dev_service_map_[it].reset(
        new Camera3Service::Camera3DeviceService(it, cb));
    int result = cam_dev_service_map_[it]->Initialize();
    if (result != 0) {
      LOG(ERROR) << "Camera device " << it << " service initialization fails";
      cam_dev_service_map_.clear();
      return result;
    }
  }
  initialized_ = true;
  return 0;
}

void Camera3Service::Destroy() {
  base::AutoLock l(lock_);
  if (!initialized_) {
    return;
  }
  for (auto& it : cam_dev_service_map_) {
    it.second->Destroy();
  }
  cam_dev_service_map_.clear();
  initialized_ = false;
}

int Camera3Service::StartPreview(int cam_id,
                                 const ResolutionInfo& preview_resolution) {
  ResolutionInfo still_capture_resolution(0, 0);
  return PrepareStillCaptureAndStartPreview(cam_id, still_capture_resolution,
                                            preview_resolution);
}

int Camera3Service::PrepareStillCaptureAndStartPreview(
    int cam_id,
    const ResolutionInfo& still_capture_resolution,
    const ResolutionInfo& preview_resolution) {
  base::AutoLock l(lock_);
  if (!initialized_ ||
      cam_dev_service_map_.find(cam_id) == cam_dev_service_map_.end()) {
    return -ENODEV;
  }
  return cam_dev_service_map_[cam_id]->PrepareStillCaptureAndStartPreview(
      still_capture_resolution, preview_resolution);
}

void Camera3Service::StopPreview(int cam_id) {
  base::AutoLock l(lock_);
  if (initialized_ &&
      cam_dev_service_map_.find(cam_id) != cam_dev_service_map_.end()) {
    cam_dev_service_map_[cam_id]->StopPreview();
  }
}

void Camera3Service::StartAutoFocus(int cam_id) {
  base::AutoLock l(lock_);
  if (initialized_ &&
      cam_dev_service_map_.find(cam_id) != cam_dev_service_map_.end()) {
    cam_dev_service_map_[cam_id]->StartAutoFocus();
  }
}

int Camera3Service::WaitForAutoFocusDone(int cam_id) {
  base::AutoLock l(lock_);
  if (!initialized_ ||
      cam_dev_service_map_.find(cam_id) == cam_dev_service_map_.end()) {
    return -ENODEV;
  }
  return cam_dev_service_map_[cam_id]->WaitForAutoFocusDone();
}

int Camera3Service::WaitForAWBConvergedAndLock(int cam_id) {
  base::AutoLock l(lock_);
  if (!initialized_ ||
      cam_dev_service_map_.find(cam_id) == cam_dev_service_map_.end()) {
    return -ENODEV;
  }
  return cam_dev_service_map_[cam_id]->WaitForAWBConvergedAndLock();
}

void Camera3Service::StartAEPrecapture(int cam_id) {
  base::AutoLock l(lock_);
  if (initialized_ &&
      cam_dev_service_map_.find(cam_id) != cam_dev_service_map_.end()) {
    cam_dev_service_map_[cam_id]->StartAEPrecapture();
  }
}

int Camera3Service::WaitForAEStable(int cam_id) {
  base::AutoLock l(lock_);
  if (!initialized_ ||
      cam_dev_service_map_.find(cam_id) == cam_dev_service_map_.end()) {
    return -ENODEV;
  }
  return cam_dev_service_map_[cam_id]->WaitForAEStable();
}

void Camera3Service::TakeStillCapture(int cam_id,
                                      const camera_metadata_t* metadata) {
  base::AutoLock l(lock_);
  if (initialized_ &&
      cam_dev_service_map_.find(cam_id) != cam_dev_service_map_.end()) {
    cam_dev_service_map_[cam_id]->TakeStillCapture(metadata);
  }
}

int Camera3Service::WaitForPreviewFrames(int cam_id,
                                         uint32_t num_frames,
                                         uint32_t timeout_ms) {
  base::AutoLock l(lock_);
  if (!initialized_ ||
      cam_dev_service_map_.find(cam_id) == cam_dev_service_map_.end()) {
    return -ENODEV;
  }
  return cam_dev_service_map_[cam_id]->WaitForPreviewFrames(num_frames,
                                                            timeout_ms);
}

const Camera3Device::StaticInfo* Camera3Service::GetStaticInfo(int cam_id) {
  base::AutoLock l(lock_);
  if (initialized_ &&
      cam_dev_service_map_.find(cam_id) != cam_dev_service_map_.end()) {
    return cam_dev_service_map_[cam_id]->GetStaticInfo();
  }
  return nullptr;
}

const camera_metadata_t* Camera3Service::ConstructDefaultRequestSettings(
    int cam_id,
    int type) {
  base::AutoLock l(lock_);
  if (initialized_ &&
      cam_dev_service_map_.find(cam_id) != cam_dev_service_map_.end()) {
    return cam_dev_service_map_[cam_id]->ConstructDefaultRequestSettings(type);
  }
  return nullptr;
}

int Camera3Service::Camera3DeviceService::Initialize() {
  Camera3Module cam_module;
  if (cam_module.Initialize() != 0) {
    LOG(ERROR) << "Camera module initialization fails";
    return -ENODEV;
  }
  if (cam_device_.Initialize(&cam_module) != 0) {
    LOG(ERROR) << "Camera device initialization fails";
    return -ENODEV;
  }
  if (!service_thread_.Start()) {
    LOG(ERROR) << "Failed to start thread";
    return -EINVAL;
  }
  cam_device_.RegisterResultMetadataOutputBufferCallback(base::Bind(
      &Camera3Service::Camera3DeviceService::ProcessResultMetadataOutputBuffers,
      base::Unretained(this)));
  repeating_preview_metadata_.reset(clone_camera_metadata(
      cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW)));
  if (!repeating_preview_metadata_) {
    LOG(ERROR) << "Failed to create preview metadata";
    return -ENOMEM;
  }
  sem_init(&preview_frame_sem_, 0, 0);
  return 0;
}

void Camera3Service::Camera3DeviceService::Destroy() {
  sem_destroy(&preview_frame_sem_);
  cam_device_.Destroy();
}

int Camera3Service::Camera3DeviceService::PrepareStillCaptureAndStartPreview(
    const ResolutionInfo& still_capture_resolution,
    const ResolutionInfo& preview_resolution) {
  VLOGF_ENTER();
  int result = -EIO;
  service_thread_.PostTaskSync(
      FROM_HERE,
      base::Bind(&Camera3Service::Camera3DeviceService::
                     PrepareStillCaptureAndStartPreviewOnServiceThread,
                 base::Unretained(this), still_capture_resolution,
                 preview_resolution, &result));
  return result;
}

void Camera3Service::Camera3DeviceService::StopPreview() {
  VLOGF_ENTER();
  auto future = internal::Future<void>::Create(nullptr);
  service_thread_.PostTaskAsync(
      FROM_HERE,
      base::Bind(
          &Camera3Service::Camera3DeviceService::StopPreviewOnServiceThread,
          base::Unretained(this), internal::GetFutureCallback(future)));
  if (!future->Wait(kWaitForStopPreviewTimeoutMs)) {
    LOG(ERROR) << "Timeout stopping preview";
  }
}

void Camera3Service::Camera3DeviceService::StartAutoFocus() {
  service_thread_.PostTaskAsync(
      FROM_HERE,
      base::Bind(
          &Camera3Service::Camera3DeviceService::StartAutoFocusOnServiceThread,
          base::Unretained(this)));
}

int Camera3Service::Camera3DeviceService::WaitForAutoFocusDone() {
  VLOGF_ENTER();
  auto future = internal::Future<void>::Create(nullptr);
  service_thread_.PostTaskAsync(
      FROM_HERE, base::Bind(&Camera3Service::Camera3DeviceService::
                                AddMetadataListenerOnServiceThread,
                            base::Unretained(this), ANDROID_CONTROL_AF_STATE,
                            ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED,
                            internal::GetFutureCallback(future)));
  return future->Wait(kWaitForFocusDoneTimeoutMs) ? 0 : -ETIMEDOUT;
}

int Camera3Service::Camera3DeviceService::WaitForAWBConvergedAndLock() {
  VLOGF_ENTER();
  auto future = internal::Future<void>::Create(nullptr);
  service_thread_.PostTaskAsync(
      FROM_HERE, base::Bind(&Camera3Service::Camera3DeviceService::
                                AddMetadataListenerOnServiceThread,
                            base::Unretained(this), ANDROID_CONTROL_AWB_STATE,
                            ANDROID_CONTROL_AWB_STATE_CONVERGED,
                            internal::GetFutureCallback(future)));
  if (!future->Wait(kWaitForAWBConvergedTimeoutMs)) {
    return -ETIMEDOUT;
  }

  if (cam_device_.GetStaticInfo()->IsAWBLockSupported()) {
    service_thread_.PostTaskAsync(
        FROM_HERE,
        base::Bind(
            &Camera3Service::Camera3DeviceService::LockAWBOnServiceThread,
            base::Unretained(this)));
  }
  return 0;
}

void Camera3Service::Camera3DeviceService::StartAEPrecapture() {
  VLOGF_ENTER();
  service_thread_.PostTaskAsync(
      FROM_HERE, base::Bind(&Camera3Service::Camera3DeviceService::
                                StartAEPrecaptureOnServiceThread,
                            base::Unretained(this)));
}

int Camera3Service::Camera3DeviceService::WaitForAEStable() {
  VLOGF_ENTER();
  auto future = internal::Future<void>::Create(nullptr);
  service_thread_.PostTaskAsync(
      FROM_HERE, base::Bind(&Camera3Service::Camera3DeviceService::
                                AddMetadataListenerOnServiceThread,
                            base::Unretained(this), ANDROID_CONTROL_AE_STATE,
                            ANDROID_CONTROL_AE_STATE_CONVERGED,
                            internal::GetFutureCallback(future)));
  return future->Wait(kWaitForFocusDoneTimeoutMs) ? 0 : -ETIMEDOUT;
}

void Camera3Service::Camera3DeviceService::TakeStillCapture(
    const camera_metadata_t* metadata) {
  VLOGF_ENTER();
  auto future = internal::Future<void>::Create(nullptr);
  service_thread_.PostTaskAsync(
      FROM_HERE, base::Bind(&Camera3Service::Camera3DeviceService::
                                TakeStillCaptureOnServiceThread,
                            base::Unretained(this), metadata,
                            internal::GetFutureCallback(future)));
  // Wait for ProcessPreviewRequestOnServiceThread() to finish processing
  // |metadata|
  future->Wait();
}

int Camera3Service::Camera3DeviceService::WaitForPreviewFrames(
    uint32_t num_frames,
    uint32_t timeout_ms) {
  VLOGF_ENTER();
  while (sem_trywait(&preview_frame_sem_) == 0)
    ;
  for (uint32_t i = 0; i < num_frames; ++i) {
    struct timespec timeout = {};
    if (clock_gettime(CLOCK_REALTIME, &timeout)) {
      LOGF(ERROR) << "Failed to get clock time";
      return -errno;
    }
    timeout.tv_sec += timeout_ms / 1000;
    timeout.tv_nsec += (timeout_ms % 1000) * 1000;
    if (sem_timedwait(&preview_frame_sem_, &timeout) != 0) {
      return -errno;
    }
  }
  return 0;
}

const Camera3Device::StaticInfo*
Camera3Service::Camera3DeviceService::GetStaticInfo() const {
  return cam_device_.GetStaticInfo();
}

const camera_metadata_t*
Camera3Service::Camera3DeviceService::ConstructDefaultRequestSettings(
    int type) {
  return cam_device_.ConstructDefaultRequestSettings(type);
}

void Camera3Service::Camera3DeviceService::ProcessResultMetadataOutputBuffers(
    uint32_t frame_number,
    CameraMetadataUniquePtr metadata,
    std::vector<BufferHandleUniquePtr> buffers) {
  VLOGF_ENTER();
  service_thread_.PostTaskAsync(
      FROM_HERE,
      base::Bind(&Camera3Service::Camera3DeviceService::
                     ProcessResultMetadataOutputBuffersOnServiceThread,
                 base::Unretained(this), frame_number, base::Passed(&metadata),
                 base::Passed(&buffers)));
}

void Camera3Service::Camera3DeviceService::
    PrepareStillCaptureAndStartPreviewOnServiceThread(
        const ResolutionInfo still_capture_resolution,
        const ResolutionInfo preview_resolution,
        int* result) {
  DCHECK(service_thread_.IsCurrentThread());
  VLOGF_ENTER();
  if (preview_state_ != PREVIEW_STOPPED) {
    LOG(ERROR) << "Failed to start preview because it is not stopped";
    *result = -EAGAIN;
    return;
  }

  if (still_capture_resolution.Area()) {
    cam_device_.AddOutputStream(HAL_PIXEL_FORMAT_BLOB,
                                still_capture_resolution.Width(),
                                still_capture_resolution.Height());
  }
  cam_device_.AddOutputStream(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                              preview_resolution.Width(),
                              preview_resolution.Height());
  if (cam_device_.ConfigureStreams(&streams_) != 0) {
    ADD_FAILURE() << "Configuring stream fails";
    *result = -EINVAL;
    return;
  }
  auto it = std::find_if(
      streams_.begin(), streams_.end(), [](const camera3_stream_t* stream) {
        return (stream->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
      });
  if (it == streams_.end()) {
    ADD_FAILURE() << "Failed to find configured preview stream";
    *result = -EINVAL;
    return;
  }
  const camera3_stream_t* preview_stream = *it;

  number_of_capture_requests_ = preview_stream->max_buffers;
  capture_requests_.resize(number_of_capture_requests_);
  output_stream_buffers_ = std::vector<std::vector<camera3_stream_buffer_t>>(
      number_of_capture_requests_,
      std::vector<camera3_stream_buffer_t>(kNumberOfOutputStreamBuffers));
  // Submit initial preview capture requests to fill the HAL pipeline first.
  // Then when a result callback is processed, the corresponding capture
  // request (and output buffer) is recycled and submitted again.
  for (size_t i = 0; i < number_of_capture_requests_; i++) {
    std::vector<const camera3_stream_t*> streams(1, preview_stream);
    std::vector<camera3_stream_buffer_t> output_buffers;
    if (cam_device_.AllocateOutputBuffersByStreams(streams, &output_buffers) !=
        0) {
      ADD_FAILURE() << "Failed to allocate output buffer";
      *result = -EINVAL;
      return;
    }
    output_stream_buffers_[i][kPreviewOutputStreamIdx] = output_buffers.front();
    capture_requests_[i] = {
        .frame_number = UINT32_MAX,  // Will be overwritten with correct value
        .settings = repeating_preview_metadata_.get(),
        .input_buffer = NULL,
        .num_output_buffers = 1,
        .output_buffers = output_stream_buffers_[i].data()};
    ProcessPreviewRequestOnServiceThread();
  }
  preview_state_ = PREVIEW_STARTED;
  *result = 0;
}

void Camera3Service::Camera3DeviceService::StopPreviewOnServiceThread(
    base::Callback<void()> cb) {
  DCHECK(service_thread_.IsCurrentThread());
  VLOGF_ENTER();
  if (preview_state_ != PREVIEW_STARTED) {
    return;
  }
  preview_state_ = PREVIEW_STOPPING;
  stop_preview_cb_ = cb;
}

void Camera3Service::Camera3DeviceService::StartAutoFocusOnServiceThread() {
  DCHECK(service_thread_.IsCurrentThread());
  uint8_t af_mode = ANDROID_CONTROL_AF_MODE_AUTO;
  EXPECT_EQ(0, UpdateMetadata(ANDROID_CONTROL_AF_MODE, &af_mode,
                              sizeof(af_mode), &repeating_preview_metadata_));
  if (!oneshot_preview_metadata_.get()) {
    oneshot_preview_metadata_.reset(
        clone_camera_metadata(repeating_preview_metadata_.get()));
  }
  uint8_t af_trigger = ANDROID_CONTROL_AF_TRIGGER_START;
  EXPECT_EQ(0, UpdateMetadata(ANDROID_CONTROL_AF_TRIGGER, &af_trigger,
                              sizeof(af_trigger), &oneshot_preview_metadata_));
}

void Camera3Service::Camera3DeviceService::AddMetadataListenerOnServiceThread(
    int32_t key,
    int32_t value,
    base::Callback<void()> cb) {
  DCHECK(service_thread_.IsCurrentThread());
  metadata_listener_list_.emplace_back(key, value, cb);
}

void Camera3Service::Camera3DeviceService::LockAWBOnServiceThread() {
  DCHECK(service_thread_.IsCurrentThread());
  uint8_t awb_lock = ANDROID_CONTROL_AWB_LOCK_ON;
  EXPECT_EQ(0, UpdateMetadata(ANDROID_CONTROL_AWB_LOCK, &awb_lock,
                              sizeof(awb_lock), &repeating_preview_metadata_));
}

void Camera3Service::Camera3DeviceService::StartAEPrecaptureOnServiceThread() {
  DCHECK(service_thread_.IsCurrentThread());
  if (!oneshot_preview_metadata_.get()) {
    oneshot_preview_metadata_.reset(
        clone_camera_metadata(repeating_preview_metadata_.get()));
  }
  uint8_t ae_trigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_START;
  EXPECT_EQ(0,
            UpdateMetadata(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER, &ae_trigger,
                           sizeof(ae_trigger), &oneshot_preview_metadata_));
}

void Camera3Service::Camera3DeviceService::TakeStillCaptureOnServiceThread(
    const camera_metadata_t* metadata,
    base::Callback<void()> cb) {
  DCHECK(service_thread_.IsCurrentThread());
  still_capture_metadata_ = metadata;
  still_capture_cb_ = cb;
}

void Camera3Service::Camera3DeviceService::
    ProcessPreviewRequestOnServiceThread() {
  DCHECK(service_thread_.IsCurrentThread());
  camera3_capture_request_t* request = &capture_requests_[capture_request_idx_];
  if (still_capture_metadata_) {
    auto it = std::find_if(streams_.begin(), streams_.end(),
                           [](const camera3_stream_t* stream) {
                             return (stream->format == HAL_PIXEL_FORMAT_BLOB);
                           });
    ASSERT_NE(streams_.end(), it)
        << "Failed to find configured still capture stream";
    const camera3_stream_t* still_capture_stream = *it;
    std::vector<const camera3_stream_t*> streams(1, still_capture_stream);
    std::vector<camera3_stream_buffer_t> output_buffers;
    ASSERT_EQ(
        0, cam_device_.AllocateOutputBuffersByStreams(streams, &output_buffers))
        << "Failed to allocate output buffer";
    request->settings = still_capture_metadata_;
    request->num_output_buffers = 2;  // preview + still capture
    output_stream_buffers_[capture_request_idx_][kStillCaptureOutputStreamIdx] =
        output_buffers.front();
  } else {
    // Request with one-shot metadata if there is one
    request->settings = oneshot_preview_metadata_.get()
                            ? oneshot_preview_metadata_.get()
                            : repeating_preview_metadata_.get();
    request->num_output_buffers = 1;  // preview only
  }
  ASSERT_EQ(0, cam_device_.ProcessCaptureRequest(request))
      << "Failed to process capture request";
  ++number_of_in_flight_requests_;
  VLOGF(1) << "Capture request";
  VLOGF(1) << "  Frame " << request->frame_number;
  VLOGF(1) << "  Index " << capture_request_idx_;
  for (size_t i = 0; i < request->num_output_buffers; i++) {
    VLOGF(1) << "  Buffer " << *request->output_buffers[i].buffer
             << " (format:" << request->output_buffers[i].stream->format << ","
             << request->output_buffers[i].stream->width << "x"
             << request->output_buffers[i].stream->height << ")";
  }
  VLOGF(1) << "  Settings " << request->settings;
  if (still_capture_metadata_) {
    still_capture_metadata_ = nullptr;
    still_capture_cb_.Run();
  } else if (oneshot_preview_metadata_.get()) {
    oneshot_preview_metadata_.reset(nullptr);
  }
  INCREASE_INDEX(capture_request_idx_);
}

void Camera3Service::Camera3DeviceService::
    ProcessResultMetadataOutputBuffersOnServiceThread(
        uint32_t frame_number,
        CameraMetadataUniquePtr metadata,
        std::vector<BufferHandleUniquePtr> buffers) {
  DCHECK(service_thread_.IsCurrentThread());
  --number_of_in_flight_requests_;
  size_t capture_request_idx = 0;
  for (; capture_request_idx < capture_requests_.size();
       capture_request_idx++) {
    if (capture_requests_[capture_request_idx].frame_number == frame_number) {
      break;
    }
  }
  VLOGF(1) << "Capture result";
  VLOGF(1) << "  Frame " << frame_number;
  VLOGF(1) << "  Index " << capture_request_idx;
  // Process result metadata according to listeners
  for (auto it = metadata_listener_list_.begin();
       it != metadata_listener_list_.end();) {
    camera_metadata_ro_entry_t entry;
    if (find_camera_metadata_ro_entry(metadata.get(), it->key, &entry) == 0 &&
        entry.data.i32[0] == it->value) {
      VLOGF(1) << "Metadata listener gets tag "
               << get_camera_metadata_tag_name(it->key) << " value "
               << it->value;
      it->cb.Run();
      it = metadata_listener_list_.erase(it);
    } else {
      it++;
    }
  }
  // Process output buffers
  bool stopping_preview =
      (preview_state_ == PREVIEW_STOPPING) && !still_capture_metadata_;
  for (auto& it : buffers) {
    VLOGF(1) << "  Buffer " << *it
             << " (format:" << Camera3TestGralloc::GetFormat(*it) << ")";
    if (Camera3TestGralloc::GetFormat(*it) == HAL_PIXEL_FORMAT_BLOB) {
      if (!process_still_capture_result_cb_.is_null()) {
        process_still_capture_result_cb_.Run(
            cam_id_, frame_number, std::move(metadata), std::move(it));
      }
    } else if (!stopping_preview) {
      // Register buffer back to be used by future requests
      cam_device_.RegisterOutputBuffer(
          *output_stream_buffers_[capture_request_idx][kPreviewOutputStreamIdx]
               .stream,
          std::move(it));
    }
  }
  sem_post(&preview_frame_sem_);

  if (stopping_preview) {
    VLOGF(1) << "Stopping preview ... (" << number_of_in_flight_requests_
             << " requests in flight";
    if (number_of_in_flight_requests_ == 0) {
      preview_state_ = PREVIEW_STOPPED;
      capture_request_idx_ = 0;
      stop_preview_cb_.Run();
    }
    return;
  }
  ProcessPreviewRequestOnServiceThread();
}

}  // namespace camera3_test
