// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sync/sync.h>

#include "camera3_frame_fixture.h"

namespace camera3_test {

void Camera3FrameFixture::SetUp() {
  Camera3StreamFixture::SetUp();

  // Initialize semaphores for frame captures
  sem_init(&shutter_sem_, 0, 0);
  sem_init(&capture_result_sem_, 0, 0);
}

void Camera3FrameFixture::TearDown() {
  sem_destroy(&shutter_sem_);
  sem_destroy(&capture_result_sem_);

  Camera3StreamFixture::TearDown();
}

int32_t Camera3FrameFixture::CreateCaptureRequest(int32_t type) {
  // Allocate output buffers
  int32_t ret;
  std::vector<camera3_stream_buffer_t> output_buffers;
  EXPECT_EQ(0, cam_device_.AllocateOutputStreamBuffers(&output_buffers))
      << "Failed to allocate buffers for capture request";
  if (HasFailure()) {
    return -EINVAL;
  }

  // Use default metadata settings
  const camera_metadata_t* default_settings;
  default_settings = cam_device_.ConstructDefaultRequestSettings(type);
  EXPECT_NE(nullptr, default_settings) << "Camera default settings are NULL";
  if (HasFailure()) {
    return -EINVAL;
  }

  // Keep capture request to verify capture result
  capture_request_[request_frame_number_] = {
      .frame_number = request_frame_number_,
      .settings = default_settings,
      .input_buffer = NULL,
      .num_output_buffers = output_buffers.size(),
      .output_buffers = output_buffers.data()};

  // Process capture request
  ret = cam_device_.ProcessCaptureRequest(
      &capture_request_[request_frame_number_]);

  request_frame_number_++;

  return ret;
}

void Camera3FrameFixture::WaitCaptureResult(int32_t ms) {
  struct timespec ts;

  // Setup timeout
  clock_gettime(CLOCK_REALTIME, &ts);
  ts.tv_sec += ms / 1000;
  ts.tv_nsec += (ms % 1000) * 1000;

  // Wait for shutter callback
  ASSERT_EQ(0, sem_timedwait(&shutter_sem_, &ts));

  // Wait for capture result callback
  ASSERT_EQ(0, sem_timedwait(&capture_result_sem_, &ts));
}

void Camera3FrameFixture::ProcessCaptureResult(
    const camera3_capture_result* result) {
  ASSERT_NE(nullptr, result) << "Capture result is null";
  ASSERT_TRUE((result->result != NULL) || (result->num_output_buffers != 0) ||
              (result->input_buffer != NULL))
      << "No result data provided by HAL for frame " << result->frame_number;

  for (uint32_t i = 0; i < result->num_output_buffers; i++) {
    camera3_stream_buffer_t stream_buffer = result->output_buffers[i];
    ASSERT_NE(nullptr, stream_buffer.buffer)
        << "Capture result output buffer is null";
    ASSERT_EQ(CAMERA3_BUFFER_STATUS_OK, stream_buffer.status)
        << "Capture result buffer status error";
    ASSERT_EQ(-1, stream_buffer.acquire_fence)
        << "Capture result buffer fence error";

    // TODO: wait sync in threads
    // TODO: check buffers for a given streams are returned in order
    if (stream_buffer.release_fence != -1) {
      ASSERT_EQ(0, sync_wait(stream_buffer.release_fence, 1000))
          << "Error waiting on buffer acquire fence";
      close(stream_buffer.release_fence);
    }
  }

  // Verify and free buffers
  std::vector<camera3_stream_buffer_t> output_buffers(
      result->output_buffers,
      result->output_buffers + result->num_output_buffers);
  EXPECT_EQ(0, cam_device_.FreeOutputStreamBuffers(output_buffers))
      << "Buffer returned in capture result is invalid";

  capture_result_[result->frame_number].num_output_buffers +=
      result->num_output_buffers;
  if (capture_result_[result->frame_number].num_output_buffers ==
      capture_request_[result->frame_number].num_output_buffers) {
    // All output buufers are ready
    ASSERT_EQ(result->frame_number, result_frame_number_)
        << "Capture result is out of order";
    result_frame_number_++;

    // Everything looks fine. Post it now.
    sem_post(&capture_result_sem_);
  }
}

void Camera3FrameFixture::Notify(const camera3_notify_msg* msg) {
  ASSERT_EQ(CAMERA3_MSG_SHUTTER, msg->type) << "Shutter error = "
                                            << msg->message.error.error_code;

  sem_post(&shutter_sem_);
}

// Test parameters:
// - Camera ID
// - Frame format
// - If true, capture with the maximum resolution supported for this format;
// otherwise, capture the minimum one.
class Camera3FrameTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<std::tuple<int32_t, int32_t, bool>> {
 public:
  Camera3FrameTest() : Camera3FrameFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3FrameTest, GetFrame) {
  int32_t format = std::get<1>(GetParam());

  if (cam_module_.IsFormatAvailable(cam_id_, format)) {
    ResolutionInfo resolution(0, 0);

    if (std::get<2>(GetParam())) {
      ASSERT_EQ(0, GetMaxResolution(format, &resolution))
          << "Failed to get max resolution for format " << format;
    } else {
      ASSERT_EQ(0, GetMinResolution(format, &resolution))
          << "Failed to get min resolution for format " << format;
    }

    cam_device_.AddOutputStream(format, resolution.Width(),
                                resolution.Height());
    ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

    ASSERT_EQ(0, CreateCaptureRequest(CAMERA3_TEMPLATE_STILL_CAPTURE))
        << "Creating capture request fails";

    WaitCaptureResult(1000);
  }
}

// Test parameters:
// - Camera ID
// - Number of frames to capture
class Camera3MultiFrameTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<std::tuple<int32_t, int32_t>> {
 public:
  Camera3MultiFrameTest() : Camera3FrameFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3MultiFrameTest, GetFrame) {
  cam_device_.AddOutputStream(HAL_PIXEL_FORMAT_YCbCr_420_888, default_width_,
                              default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

  int32_t num_frames = std::get<1>(GetParam());
  for (int32_t i = 0; i < num_frames; i++) {
    ASSERT_EQ(0, CreateCaptureRequest(CAMERA3_TEMPLATE_PREVIEW))
        << "Creating capture request fails";
  }

  for (int32_t i = 0; i < num_frames; i++) {
    WaitCaptureResult(1000);
  }
}

// Test parameters:
// - Camera ID
class Camera3MultiStreamFrameTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3MultiStreamFrameTest() : Camera3FrameFixture(GetParam()) {}
};

TEST_P(Camera3MultiStreamFrameTest, GetFrame) {
  // Preview stream with large size no bigger than 1080p
  ResolutionInfo limit_resolution(1920, 1080);
  ResolutionInfo preview_resolution(0, 0);
  ASSERT_EQ(0, GetMaxResolution(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                                &preview_resolution))
      << "Failed to get max resolution for implementation defined format";
  preview_resolution = CapResolution(preview_resolution, limit_resolution);
  cam_device_.AddOutputStream(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                              preview_resolution.Width(),
                              preview_resolution.Height());

  // Face detection stream with small size
  ResolutionInfo fd_resolution(0, 0);
  ASSERT_EQ(0, GetMinResolution(HAL_PIXEL_FORMAT_YCbCr_420_888, &fd_resolution))
      << "Failed to get min resolution for YCbCr 420 format";
  cam_device_.AddOutputStream(HAL_PIXEL_FORMAT_YCbCr_420_888,
                              fd_resolution.Width(), fd_resolution.Height());

  // Capture stream with largest size
  ResolutionInfo capture_resolution(0, 0);
  ASSERT_EQ(
      0, GetMaxResolution(HAL_PIXEL_FORMAT_YCbCr_420_888, &capture_resolution))
      << "Failed to get max resolution for YCbCr 420 format";
  cam_device_.AddOutputStream(HAL_PIXEL_FORMAT_YCbCr_420_888,
                              capture_resolution.Width(),
                              capture_resolution.Height());

  ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

  ASSERT_EQ(0, CreateCaptureRequest(CAMERA3_TEMPLATE_PREVIEW))
      << "Creating capture request fails";

  WaitCaptureResult(1000);
}

// TODO: flush test

INSTANTIATE_TEST_CASE_P(
    GetFrame,
    Camera3FrameTest,
    ::testing::Combine(
        ::testing::ValuesIn(Camera3Module().GetCameraIds()),
        ::testing::Values(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                          HAL_PIXEL_FORMAT_YCbCr_420_888,
                          HAL_PIXEL_FORMAT_YCrCb_420_SP,
                          HAL_PIXEL_FORMAT_BLOB,
                          HAL_PIXEL_FORMAT_YV12,
                          HAL_PIXEL_FORMAT_Y8,
                          HAL_PIXEL_FORMAT_Y16,
                          HAL_PIXEL_FORMAT_RAW16),
        ::testing::Bool()));

INSTANTIATE_TEST_CASE_P(
    GetFrame,
    Camera3MultiFrameTest,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Range(1, 10)));

INSTANTIATE_TEST_CASE_P(GetFrame,
                        Camera3MultiStreamFrameTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

}  // namespace camera3_test
