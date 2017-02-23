// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sync/sync.h>

#include "camera3_frame_fixture.h"

namespace camera3_test {

static void ExpectKeyValueGreaterThanI64(const camera_metadata_t* settings,
                                         int32_t key,
                                         const char* key_name,
                                         int64_t value) {
  camera_metadata_ro_entry_t entry;
  ASSERT_EQ(0, find_camera_metadata_ro_entry(settings, key, &entry))
      << "Cannot find the metadata " << key_name;
  ASSERT_GT(entry.data.i64[0], value) << "Wrong value of metadata " << key_name;
}
#define EXPECT_KEY_VALUE_GT_I64(settings, key, value) \
  ExpectKeyValueGreaterThanI64(settings, key, #key, value)

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
      .num_output_buffers = static_cast<uint32_t>(output_buffers.size()),
      .output_buffers = output_buffers.data()};

  // Process capture request
  ret = cam_device_.ProcessCaptureRequest(
      &capture_request_[request_frame_number_]);

  request_frame_number_++;

  return ret;
}

void Camera3FrameFixture::WaitCaptureResult(const struct timespec& timeout) {
  // Wait for shutter callback
  ASSERT_EQ(0, sem_timedwait(&shutter_sem_, &timeout));

  // Wait for capture result callback
  ASSERT_EQ(0, sem_timedwait(&capture_result_sem_, &timeout));
}

int Camera3FrameFixture::GetNumberOfCaptureResults() {
  int num_capture_results = 0;
  sem_getvalue(&capture_result_sem_, &num_capture_results);
  return num_capture_results;
}

void Camera3FrameFixture::ProcessCaptureResult(
    const camera3_capture_result* result) {
  ASSERT_NE(nullptr, result) << "Capture result is null";
  // At least one of metadata or output buffers or input buffer should be
  // returned
  ASSERT_TRUE((result->result != NULL) || (result->num_output_buffers != 0) ||
              (result->input_buffer != NULL))
      << "No result data provided by HAL for frame " << result->frame_number;
  if (result->num_output_buffers) {
    ASSERT_NE(nullptr, result->output_buffers)
        << "No output buffer is returned while " << result->num_output_buffers
        << " are expected";
  }

  if (result->result) {
    EXPECT_KEY_VALUE_GT_I64(result->result, ANDROID_SENSOR_TIMESTAMP, 0);
  }

  for (uint32_t i = 0; i < result->num_output_buffers; i++) {
    camera3_stream_buffer_t stream_buffer = result->output_buffers[i];
    ASSERT_NE(nullptr, stream_buffer.buffer)
        << "Capture result output buffer is null";
    // An error may be expected while flushing
    EXPECT_EQ(CAMERA3_BUFFER_STATUS_OK, stream_buffer.status)
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
  // An error may be expected while flushing
  EXPECT_EQ(CAMERA3_MSG_SHUTTER, msg->type) << "Shutter error = "
                                            << msg->message.error.error_code;

  sem_post(&shutter_sem_);
}

// Get real-time clock time after waiting for given timeout
static void GetTimeOfTimeout(int32_t ms, struct timespec* ts) {
  clock_gettime(CLOCK_REALTIME, ts);
  ts->tv_sec += ms / 1000;
  ts->tv_nsec += (ms % 1000) * 1000;
}

// Test parameters:
// - Camera ID
// - Template ID
// - Frame format
// - If true, capture with the maximum resolution supported for this format;
// otherwise, capture the minimum one.
class Camera3FrameTest : public Camera3FrameFixture,
                         public ::testing::WithParamInterface<
                             std::tuple<int32_t, int32_t, int32_t, bool>> {
 public:
  Camera3FrameTest() : Camera3FrameFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3FrameTest, GetFrame) {
  int32_t format = std::get<1>(GetParam());
  int32_t type = std::get<2>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  if (cam_module_.IsFormatAvailable(cam_id_, format)) {
    ResolutionInfo resolution(0, 0);
    if (std::get<3>(GetParam())) {
      ASSERT_EQ(0, GetMaxResolution(format, &resolution))
          << "Failed to get max resolution for format " << format;
    } else {
      ASSERT_EQ(0, GetMinResolution(format, &resolution))
          << "Failed to get min resolution for format " << format;
    }

    cam_device_.AddOutputStream(format, resolution.Width(),
                                resolution.Height());
    ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

    ASSERT_EQ(0, CreateCaptureRequest(type))
        << "Creating capture request fails";

    struct timespec timeout;
    GetTimeOfTimeout(1000, &timeout);
    WaitCaptureResult(timeout);
  }
}

// Test parameters:
// - Camera ID
// - Template ID
// - Number of frames to capture
class Camera3MultiFrameTest : public Camera3FrameFixture,
                              public ::testing::WithParamInterface<
                                  std::tuple<int32_t, int32_t, int32_t>> {
 public:
  Camera3MultiFrameTest() : Camera3FrameFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3MultiFrameTest, GetFrame) {
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

  int32_t type = std::get<1>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  int32_t num_frames = std::get<2>(GetParam());
  for (int32_t i = 0; i < num_frames; i++) {
    EXPECT_EQ(0, CreateCaptureRequest(type))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  for (int32_t i = 0; i < num_frames; i++) {
    WaitCaptureResult(timeout);
  }
}

// Test parameters:
// - Camera ID
class Camera3MixedTemplateMultiFrameTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3MixedTemplateMultiFrameTest() : Camera3FrameFixture(GetParam()) {}
};

TEST_P(Camera3MixedTemplateMultiFrameTest, GetFrame) {
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

  int32_t types[] = {CAMERA3_TEMPLATE_PREVIEW, CAMERA3_TEMPLATE_STILL_CAPTURE,
                     CAMERA3_TEMPLATE_VIDEO_RECORD,
                     CAMERA3_TEMPLATE_VIDEO_SNAPSHOT};
  for (size_t i = 0; i < arraysize(types); ++i) {
    EXPECT_EQ(0, CreateCaptureRequest(types[i]))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  for (size_t i = 0; i < arraysize(types); ++i) {
    WaitCaptureResult(timeout);
  }
}

// Test parameters:
// - Camera ID
// - Template ID
// - Number of frames to capture
class Camera3FlushRequestsTest : public Camera3FrameFixture,
                                 public ::testing::WithParamInterface<
                                     std::tuple<int32_t, int32_t, int32_t>> {
 public:
  Camera3FlushRequestsTest() : Camera3FrameFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3FlushRequestsTest, GetFrame) {
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

  int32_t type = std::get<1>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  const int32_t num_frames = std::get<2>(GetParam());
  for (int32_t i = 0; i < num_frames; i++) {
    EXPECT_EQ(0, CreateCaptureRequest(type))
        << "Creating capture request fails";
  }

  ASSERT_EQ(0, cam_device_.Flush()) << "Flushing capture requests fails";

  // flush() should only return when there are no more outstanding buffers or
  // requests left in the HAL
  ASSERT_EQ(num_frames, GetNumberOfCaptureResults())
      << "There are requests left in the HAL after flushing";

  // Ignore possible errors caused by flushing
  SUCCEED();
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

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  WaitCaptureResult(timeout);
}

// Test parameters:
// - Camera ID
class Camera3InvalidRequestTest : public Camera3FrameFixture,
                                  public ::testing::WithParamInterface<int> {
 public:
  Camera3InvalidRequestTest() : Camera3FrameFixture(GetParam()) {}
};

TEST_P(Camera3InvalidRequestTest, NullOrUnconfiguredRequest) {
  // Reference:
  // camera2/cts/CameraDeviceTest.java#testInvalidCapture
  EXPECT_NE(0, cam_device_.ProcessCaptureRequest(nullptr))
      << "Capturing with null request should fail";

  const camera_metadata_t* default_settings;
  default_settings =
      cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW);
  std::vector<camera3_stream_buffer_t> output_buffers;
  std::vector<camera3_stream_t> streams(1);
  streams[0].stream_type = CAMERA3_STREAM_OUTPUT;
  streams[0].width = static_cast<uint32_t>(default_width_);
  streams[0].height = static_cast<uint32_t>(default_height_);
  streams[0].format = default_format_;
  ASSERT_EQ(0, cam_device_.AllocateOutputStreamBuffersByStreams(
                   streams, &output_buffers))
      << "Failed to allocate buffers for capture request";
  camera3_capture_request_t capture_request = {
      .frame_number = 0,
      .settings = default_settings,
      .input_buffer = NULL,
      .num_output_buffers = static_cast<uint32_t>(output_buffers.size()),
      .output_buffers = output_buffers.data()};
  EXPECT_NE(0, cam_device_.ProcessCaptureRequest(&capture_request))
      << "Capturing with stream unconfigured should fail";
  cam_device_.FreeOutputStreamBuffers(output_buffers);
}

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
        ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                          CAMERA3_TEMPLATE_STILL_CAPTURE,
                          CAMERA3_TEMPLATE_VIDEO_RECORD,
                          CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
                          CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
                          CAMERA3_TEMPLATE_MANUAL),
        ::testing::Bool()));

INSTANTIATE_TEST_CASE_P(
    GetFrame,
    Camera3MultiFrameTest,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                                         CAMERA3_TEMPLATE_STILL_CAPTURE,
                                         CAMERA3_TEMPLATE_VIDEO_RECORD,
                                         CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
                                         CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
                                         CAMERA3_TEMPLATE_MANUAL),
                       ::testing::Range(1, 10)));

INSTANTIATE_TEST_CASE_P(GetFrame,
                        Camera3MixedTemplateMultiFrameTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(
    GetFrame,
    Camera3FlushRequestsTest,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                                         CAMERA3_TEMPLATE_STILL_CAPTURE,
                                         CAMERA3_TEMPLATE_VIDEO_RECORD,
                                         CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
                                         CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
                                         CAMERA3_TEMPLATE_MANUAL),
                       ::testing::Values(10)));

INSTANTIATE_TEST_CASE_P(GetFrame,
                        Camera3MultiStreamFrameTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(NullOrUnconfiguredRequest,
                        Camera3InvalidRequestTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

}  // namespace camera3_test
