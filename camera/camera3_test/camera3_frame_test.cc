// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_frame_fixture.h"

#include <semaphore.h>

#include <list>

#include "base/macros.h"

namespace camera3_test {

int32_t Camera3FrameFixture::CreateCaptureRequest(
    const camera_metadata_t& metadata,
    uint32_t* frame_number) {
  // Allocate output buffers
  std::vector<camera3_stream_buffer_t> output_buffers;
  if (cam_device_.AllocateOutputStreamBuffers(&output_buffers)) {
    ADD_FAILURE() << "Failed to allocate buffers for capture request";
    return -EINVAL;
  }

  camera3_capture_request_t capture_request = {
      .frame_number = UINT32_MAX,
      .settings = &metadata,
      .input_buffer = NULL,
      .num_output_buffers = static_cast<uint32_t>(output_buffers.size()),
      .output_buffers = output_buffers.data()};

  // Process capture request
  int32_t ret = cam_device_.ProcessCaptureRequest(&capture_request);
  if (ret == 0 && frame_number) {
    *frame_number = capture_request.frame_number;
  }
  return ret;
}

int32_t Camera3FrameFixture::CreateCaptureRequestByMetadata(
    const CameraMetadataUniquePtr& metadata,
    uint32_t* frame_number) {
  return CreateCaptureRequest(*metadata, frame_number);
}

int32_t Camera3FrameFixture::CreateCaptureRequestByTemplate(
    int32_t type,
    uint32_t* frame_number) {
  const camera_metadata_t* default_settings;
  default_settings = cam_device_.ConstructDefaultRequestSettings(type);
  if (!default_settings) {
    ADD_FAILURE() << "Camera default settings are NULL";
    return -EINVAL;
  }

  return CreateCaptureRequest(*default_settings, frame_number);
}

void Camera3FrameFixture::WaitShutterAndCaptureResult(
    const struct timespec& timeout) {
  ASSERT_EQ(0, cam_device_.WaitShutter(timeout))
      << "Timeout waiting for shutter callback";
  ASSERT_EQ(0, cam_device_.WaitCaptureResult(timeout))
      << "Timeout waiting for capture result callback";
}

// Get real-time clock time after waiting for given timeout
static void GetTimeOfTimeout(int32_t ms, struct timespec* ts) {
  memset(ts, 0, sizeof(*ts));
  if (clock_gettime(CLOCK_REALTIME, ts)) {
    LOG(ERROR) << "Failed to get clock time";
  }
  ts->tv_sec += ms / 1000;
  ts->tv_nsec += (ms % 1000) * 1000;
}

// Test parameters:
// - Camera ID
// - Template ID
// - Frame format
// - If true, capture with the maximum resolution supported for this format;
// otherwise, capture the minimum one.
class Camera3SingleFrameTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<
          std::tuple<int32_t, int32_t, int32_t, bool>> {
 public:
  Camera3SingleFrameTest() : Camera3FrameFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3SingleFrameTest, GetFrame) {
  int32_t format = std::get<1>(GetParam());
  int32_t type = std::get<2>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  if (cam_device_.GetStaticInfo()->IsFormatAvailable(format)) {
    ResolutionInfo resolution(0, 0);
    if (std::get<3>(GetParam())) {
      ASSERT_EQ(0, GetMaxResolution(format, &resolution))
          << "Failed to get max resolution for format " << format;
    } else {
      ASSERT_EQ(0, GetMinResolution(format, &resolution))
          << "Failed to get min resolution for format " << format;
    }
    VLOGF(1) << "Device " << cam_id_;
    VLOGF(1) << "Format 0x" << std::hex << format;
    VLOGF(1) << "Resolution " << resolution.Width() << "x"
             << resolution.Height();

    cam_device_.AddOutputStream(format, resolution.Width(),
                                resolution.Height());
    ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
        << "Configuring stream fails";

    ASSERT_EQ(0, CreateCaptureRequestByTemplate(type, nullptr))
        << "Creating capture request fails";

    struct timespec timeout;
    GetTimeOfTimeout(1000, &timeout);
    WaitShutterAndCaptureResult(timeout);
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
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  int32_t type = std::get<1>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  int32_t num_frames = std::get<2>(GetParam());
  for (int32_t i = 0; i < num_frames; i++) {
    EXPECT_EQ(0, CreateCaptureRequestByTemplate(type, nullptr))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  for (int32_t i = 0; i < num_frames; i++) {
    WaitShutterAndCaptureResult(timeout);
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
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  int32_t types[] = {CAMERA3_TEMPLATE_PREVIEW, CAMERA3_TEMPLATE_STILL_CAPTURE,
                     CAMERA3_TEMPLATE_VIDEO_RECORD,
                     CAMERA3_TEMPLATE_VIDEO_SNAPSHOT};
  for (size_t i = 0; i < arraysize(types); ++i) {
    EXPECT_EQ(0, CreateCaptureRequestByTemplate(types[i], nullptr))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  for (size_t i = 0; i < arraysize(types); ++i) {
    WaitShutterAndCaptureResult(timeout);
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
  Camera3FlushRequestsTest()
      : Camera3FrameFixture(std::get<0>(GetParam())), num_capture_results_(0) {}

  virtual void SetUp() override;

 protected:
  // Callback functions from HAL device
  virtual void ProcessCaptureResult(const camera3_capture_result* result);

  // Callback functions from HAL device
  // Do nothing.
  virtual void Notify(const camera3_notify_msg* msg) {}

  // Number of received capture results with all output buffers returned
  int32_t num_capture_results_;

  sem_t flush_result_sem_;

 private:
  // Number of configured streams
  static const int32_t kNumberOfConfiguredStreams;

  // Store number of output buffers returned in capture results with frame
  // number as the key
  std::unordered_map<uint32_t, int32_t> num_capture_result_buffers_;
};

const int32_t Camera3FlushRequestsTest::kNumberOfConfiguredStreams = 1;

void Camera3FlushRequestsTest::SetUp() {
  Camera3FrameFixture::SetUp();
  cam_device_.RegisterProcessCaptureResultCallback(base::Bind(
      &Camera3FlushRequestsTest::ProcessCaptureResult, base::Unretained(this)));
  cam_device_.RegisterNotifyCallback(
      base::Bind(&Camera3FlushRequestsTest::Notify, base::Unretained(this)));
  sem_init(&flush_result_sem_, 0, 0);
}

void Camera3FlushRequestsTest::ProcessCaptureResult(
    const camera3_capture_result* result) {
  VLOGF_ENTER();
  ASSERT_NE(nullptr, result) << "Capture result is null";
  num_capture_result_buffers_[result->frame_number] +=
      result->num_output_buffers;
  if (num_capture_result_buffers_[result->frame_number] ==
      kNumberOfConfiguredStreams) {
    num_capture_results_++;
    sem_post(&flush_result_sem_);
  }
}

TEST_P(Camera3FlushRequestsTest, GetFrame) {
  // TODO: spawn a thread to test simultaneous process_capture_request and
  // flush

  // The number of configured streams must match the value of
  // |kNumberOfConfiguredStreams|.
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  int32_t type = std::get<1>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  const int32_t num_frames = std::get<2>(GetParam());
  for (int32_t i = 0; i < num_frames; i++) {
    EXPECT_EQ(0, CreateCaptureRequestByTemplate(type, nullptr))
        << "Creating capture request fails";
  }

  ASSERT_EQ(0, cam_device_.Flush()) << "Flushing capture requests fails";

  // flush() should only return when there are no more outstanding buffers or
  // requests left in the HAL
  EXPECT_EQ(num_frames, num_capture_results_)
      << "There are requests left in the HAL after flushing";

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  for (int32_t i = 0; i < num_frames; i++) {
    ASSERT_EQ(0, sem_timedwait(&flush_result_sem_, &timeout));
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

  // Another stream with small size
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

  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  ASSERT_EQ(0,
            CreateCaptureRequestByTemplate(CAMERA3_TEMPLATE_PREVIEW, nullptr))
      << "Creating capture request fails";

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  WaitShutterAndCaptureResult(timeout);
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
  std::vector<const camera3_stream_t*> stream_ptrs(1, &streams[0]);
  ASSERT_EQ(0, cam_device_.AllocateOutputBuffersByStreams(stream_ptrs,
                                                          &output_buffers))
      << "Failed to allocate buffers for capture request";
  camera3_capture_request_t capture_request = {
      .frame_number = 0,
      .settings = default_settings,
      .input_buffer = NULL,
      .num_output_buffers = static_cast<uint32_t>(output_buffers.size()),
      .output_buffers = output_buffers.data()};
  EXPECT_NE(0, cam_device_.ProcessCaptureRequest(&capture_request))
      << "Capturing with stream unconfigured should fail";
}

// Test parameters:
// - Camera ID
// - Number of frames to capture
class Camera3SimpleCaptureFrames
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<std::tuple<int32_t, int32_t>> {
 public:
  Camera3SimpleCaptureFrames()
      : Camera3FrameFixture(std::get<0>(GetParam())),
        num_frames_(std::get<1>(GetParam())) {}

 protected:
  // Process result metadata and/or output buffers
  virtual void ProcessResultMetadataOutputBuffers(
      uint32_t frame_number,
      CameraMetadataUniquePtr metadata,
      std::vector<BufferHandleUniquePtr>* buffers) override;

  // Validate capture result keys
  void ValidateCaptureResultKeys(
      const CameraMetadataUniquePtr& request_metadata);

  // Get waiver keys per camera device hardware level and capability
  void GetWaiverKeys(std::set<int32_t>* waiver_keys) const;

  // Process partial metadata
  virtual void ProcessPartialMetadata(
      std::vector<CameraMetadataUniquePtr>* partial_metadata) override;

  // Validate partial results
  void ValidatePartialMetadata();

  const int32_t num_frames_;

  // Store result metadata in the first-in-first-out order
  std::list<CameraMetadataUniquePtr> result_metadata_;

  // Store partial metadata in the first-in-first-out order
  std::list<std::vector<CameraMetadataUniquePtr>> partial_metadata_list_;

  static const int32_t kCaptureResultKeys[69];
};

void Camera3SimpleCaptureFrames::ProcessResultMetadataOutputBuffers(
    uint32_t frame_number,
    CameraMetadataUniquePtr metadata,
    std::vector<BufferHandleUniquePtr>* buffers) {
  result_metadata_.push_back(std::move(metadata));
}

void Camera3SimpleCaptureFrames::ValidateCaptureResultKeys(
    const CameraMetadataUniquePtr& request_metadata) {
  std::set<int32_t> waiver_keys;
  GetWaiverKeys(&waiver_keys);
  while (!result_metadata_.empty()) {
    camera_metadata_t* metadata = result_metadata_.front().get();
    for (auto key : kCaptureResultKeys) {
      if (waiver_keys.find(key) != waiver_keys.end()) {
        continue;
      }
      // Check the critical tags here.
      switch (key) {
        case ANDROID_CONTROL_AE_MODE:
        case ANDROID_CONTROL_AF_MODE:
        case ANDROID_CONTROL_AWB_MODE:
        case ANDROID_CONTROL_MODE:
        case ANDROID_STATISTICS_FACE_DETECT_MODE:
        case ANDROID_NOISE_REDUCTION_MODE:
          camera_metadata_ro_entry_t request_entry;
          if (find_camera_metadata_ro_entry(request_metadata.get(), key,
                                            &request_entry)) {
            ADD_FAILURE() << "Metadata " << get_camera_metadata_tag_name(key)
                          << " is unavailable in capture request";
            continue;
          }
          camera_metadata_ro_entry_t result_entry;
          if (find_camera_metadata_ro_entry(metadata, key, &result_entry)) {
            ADD_FAILURE() << "Metadata " << get_camera_metadata_tag_name(key)
                          << " is not present in capture result";
            continue;
          }
          EXPECT_EQ(request_entry.data.i32[0], result_entry.data.i32[0])
              << "Wrong value of metadata " << get_camera_metadata_tag_name(key)
              << " in capture result";
          break;
        case ANDROID_REQUEST_PIPELINE_DEPTH:
          break;
        default:
          // Only do non-null check for the rest of keys.
          camera_metadata_ro_entry_t entry;
          EXPECT_EQ(0, find_camera_metadata_ro_entry(metadata, key, &entry))
              << "Metadata " << get_camera_metadata_tag_name(key)
              << " is unavailable in capture result";
          break;
      }
    }
    result_metadata_.pop_front();
  }
}

const int32_t Camera3SimpleCaptureFrames::kCaptureResultKeys[] = {
    ANDROID_COLOR_CORRECTION_MODE,
    ANDROID_COLOR_CORRECTION_TRANSFORM,
    ANDROID_COLOR_CORRECTION_GAINS,
    ANDROID_COLOR_CORRECTION_ABERRATION_MODE,
    ANDROID_CONTROL_AE_ANTIBANDING_MODE,
    ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION,
    ANDROID_CONTROL_AE_LOCK,
    ANDROID_CONTROL_AE_MODE,
    ANDROID_CONTROL_AE_REGIONS,
    ANDROID_CONTROL_AF_REGIONS,
    ANDROID_CONTROL_AWB_REGIONS,
    ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
    ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER,
    ANDROID_CONTROL_AF_MODE,
    ANDROID_CONTROL_AF_TRIGGER,
    ANDROID_CONTROL_AWB_LOCK,
    ANDROID_CONTROL_AWB_MODE,
    ANDROID_CONTROL_CAPTURE_INTENT,
    ANDROID_CONTROL_EFFECT_MODE,
    ANDROID_CONTROL_MODE,
    ANDROID_CONTROL_SCENE_MODE,
    ANDROID_CONTROL_VIDEO_STABILIZATION_MODE,
    ANDROID_CONTROL_AE_STATE,
    ANDROID_CONTROL_AF_STATE,
    ANDROID_CONTROL_AWB_STATE,
    ANDROID_EDGE_MODE,
    ANDROID_FLASH_MODE,
    ANDROID_FLASH_STATE,
    ANDROID_HOT_PIXEL_MODE,
    ANDROID_JPEG_ORIENTATION,
    ANDROID_JPEG_QUALITY,
    ANDROID_JPEG_THUMBNAIL_QUALITY,
    ANDROID_JPEG_THUMBNAIL_SIZE,
    ANDROID_LENS_APERTURE,
    ANDROID_LENS_FILTER_DENSITY,
    ANDROID_LENS_FOCAL_LENGTH,
    ANDROID_LENS_FOCUS_DISTANCE,
    ANDROID_LENS_OPTICAL_STABILIZATION_MODE,
    ANDROID_LENS_POSE_ROTATION,
    ANDROID_LENS_POSE_TRANSLATION,
    ANDROID_LENS_FOCUS_RANGE,
    ANDROID_LENS_STATE,
    ANDROID_LENS_INTRINSIC_CALIBRATION,
    ANDROID_LENS_RADIAL_DISTORTION,
    ANDROID_NOISE_REDUCTION_MODE,
    ANDROID_REQUEST_PIPELINE_DEPTH,
    ANDROID_SCALER_CROP_REGION,
    ANDROID_SENSOR_EXPOSURE_TIME,
    ANDROID_SENSOR_FRAME_DURATION,
    ANDROID_SENSOR_SENSITIVITY,
    ANDROID_SENSOR_TIMESTAMP,
    ANDROID_SENSOR_NEUTRAL_COLOR_POINT,
    ANDROID_SENSOR_NOISE_PROFILE,
    ANDROID_SENSOR_GREEN_SPLIT,
    ANDROID_SENSOR_TEST_PATTERN_DATA,
    ANDROID_SENSOR_TEST_PATTERN_MODE,
    ANDROID_SENSOR_ROLLING_SHUTTER_SKEW,
    ANDROID_SHADING_MODE,
    ANDROID_STATISTICS_FACE_DETECT_MODE,
    ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE,
    ANDROID_STATISTICS_LENS_SHADING_CORRECTION_MAP,
    ANDROID_STATISTICS_SCENE_FLICKER,
    ANDROID_STATISTICS_HOT_PIXEL_MAP,
    ANDROID_STATISTICS_LENS_SHADING_MAP_MODE,
    ANDROID_TONEMAP_MODE,
    ANDROID_TONEMAP_GAMMA,
    ANDROID_TONEMAP_PRESET_CURVE,
    ANDROID_BLACK_LEVEL_LOCK,
    ANDROID_REPROCESS_EFFECTIVE_EXPOSURE_FACTOR};

void Camera3SimpleCaptureFrames::GetWaiverKeys(
    std::set<int32_t>* waiver_keys) const {
  // Global waiver keys
  waiver_keys->insert(ANDROID_JPEG_ORIENTATION);
  waiver_keys->insert(ANDROID_JPEG_QUALITY);
  waiver_keys->insert(ANDROID_JPEG_THUMBNAIL_QUALITY);
  waiver_keys->insert(ANDROID_JPEG_THUMBNAIL_SIZE);

  // Keys only present when corresponding control is on are being verified in
  // its own functional test
  // Only present in certain tonemap mode
  waiver_keys->insert(ANDROID_TONEMAP_CURVE_BLUE);
  waiver_keys->insert(ANDROID_TONEMAP_CURVE_GREEN);
  waiver_keys->insert(ANDROID_TONEMAP_CURVE_RED);
  waiver_keys->insert(ANDROID_TONEMAP_GAMMA);
  waiver_keys->insert(ANDROID_TONEMAP_PRESET_CURVE);
  // Only present when test pattern mode is SOLID_COLOR.
  waiver_keys->insert(ANDROID_SENSOR_TEST_PATTERN_DATA);
  // Only present when STATISTICS_LENS_SHADING_MAP_MODE is ON
  waiver_keys->insert(ANDROID_STATISTICS_LENS_SHADING_CORRECTION_MAP);
  // Only present when STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES is ON
  waiver_keys->insert(ANDROID_STATISTICS_HOT_PIXEL_MAP);
  // Only present when face detection is on
  waiver_keys->insert(ANDROID_STATISTICS_FACE_IDS);
  waiver_keys->insert(ANDROID_STATISTICS_FACE_LANDMARKS);
  waiver_keys->insert(ANDROID_STATISTICS_FACE_RECTANGLES);
  waiver_keys->insert(ANDROID_STATISTICS_FACE_SCORES);
  // Only present in reprocessing capture result.
  waiver_keys->insert(ANDROID_REPROCESS_EFFECTIVE_EXPOSURE_FACTOR);

  // Keys not required if RAW is not supported
  if (!cam_device_.GetStaticInfo()->IsCapabilitySupported(
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES_RAW)) {
    waiver_keys->insert(ANDROID_SENSOR_NEUTRAL_COLOR_POINT);
    waiver_keys->insert(ANDROID_SENSOR_GREEN_SPLIT);
    waiver_keys->insert(ANDROID_SENSOR_NOISE_PROFILE);
  }

  // TODO: CONTROL_AE_REGIONS, CONTROL_AWB_REGIONS, CONTROL_AF_REGIONS?

  if (cam_device_.GetStaticInfo()->IsHardwareLevelAtLeastFull()) {
    return;
  }

  // Keys to waive for limited devices
  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(
          ANDROID_COLOR_CORRECTION_MODE)) {
    waiver_keys->insert(ANDROID_COLOR_CORRECTION_GAINS);
    waiver_keys->insert(ANDROID_COLOR_CORRECTION_MODE);
    waiver_keys->insert(ANDROID_COLOR_CORRECTION_TRANSFORM);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(
          ANDROID_COLOR_CORRECTION_ABERRATION_MODE)) {
    waiver_keys->insert(ANDROID_COLOR_CORRECTION_ABERRATION_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(ANDROID_TONEMAP_MODE)) {
    waiver_keys->insert(ANDROID_TONEMAP_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(ANDROID_EDGE_MODE)) {
    waiver_keys->insert(ANDROID_EDGE_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(ANDROID_HOT_PIXEL_MODE)) {
    waiver_keys->insert(ANDROID_HOT_PIXEL_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(
          ANDROID_NOISE_REDUCTION_MODE)) {
    waiver_keys->insert(ANDROID_NOISE_REDUCTION_MODE);
  }

  if (!cam_device_.GetStaticInfo()->IsKeyAvailable(ANDROID_SHADING_MODE)) {
    waiver_keys->insert(ANDROID_SHADING_MODE);
  }

  // Keys not required if neither MANUAL_SENSOR nor READ_SENSOR_SETTINGS is
  // supported
  if (!cam_device_.GetStaticInfo()->IsCapabilitySupported(
          ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR)) {
    waiver_keys->insert(ANDROID_SENSOR_EXPOSURE_TIME);
    waiver_keys->insert(ANDROID_SENSOR_FRAME_DURATION);
    waiver_keys->insert(ANDROID_SENSOR_SENSITIVITY);
    waiver_keys->insert(ANDROID_BLACK_LEVEL_LOCK);
    waiver_keys->insert(ANDROID_LENS_FOCUS_RANGE);
    waiver_keys->insert(ANDROID_LENS_FOCUS_DISTANCE);
    waiver_keys->insert(ANDROID_LENS_STATE);
    waiver_keys->insert(ANDROID_LENS_APERTURE);
    waiver_keys->insert(ANDROID_LENS_FILTER_DENSITY);
  }
}

void Camera3SimpleCaptureFrames::ProcessPartialMetadata(
    std::vector<CameraMetadataUniquePtr>* partial_metadata) {
  partial_metadata_list_.resize(partial_metadata_list_.size() + 1);
  for (auto& it : *partial_metadata) {
    partial_metadata_list_.back().push_back(std::move(it));
  }
}

void Camera3SimpleCaptureFrames::ValidatePartialMetadata() {
  for (const auto& it : partial_metadata_list_) {
    // Number of partial results is less than or equal to
    // REQUEST_PARTIAL_RESULT_COUNT
    EXPECT_GE(cam_device_.GetStaticInfo()->GetPartialResultCount(), it.size())
        << "Number of received partial results is wrong";

    // Each key appeared in partial results must be unique across all partial
    // results
    for (size_t i = 0; i < it.size(); i++) {
      size_t entry_count = get_camera_metadata_entry_count(it[i].get());
      for (size_t entry_index = 0; entry_index < entry_count; entry_index++) {
        camera_metadata_ro_entry_t entry;
        ASSERT_EQ(
            0, get_camera_metadata_ro_entry(it[i].get(), entry_index, &entry));
        int32_t key = entry.tag;
        for (size_t j = i + 1; j < it.size(); j++) {
          EXPECT_NE(0, find_camera_metadata_ro_entry(it[j].get(), key, &entry))
              << "Key " << get_camera_metadata_tag_name(key)
              << " appears in multiple partial results";
        }
      }
    }
  }
}

TEST_P(Camera3SimpleCaptureFrames, Camera3ResultAllKeysTest) {
  // Reference:
  // camera2/cts/CaptureResultTest.java#testCameraCaptureResultAllKeys
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";
  CameraMetadataUniquePtr metadata(clone_camera_metadata(
      cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW)));

  for (int32_t i = 0; i < num_frames_; i++) {
    ASSERT_EQ(0, CreateCaptureRequestByMetadata(metadata, nullptr))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  for (int32_t i = 0; i < num_frames_; i++) {
    WaitShutterAndCaptureResult(timeout);
  }

  ValidateCaptureResultKeys(metadata);
}

TEST_P(Camera3SimpleCaptureFrames, Camera3PartialResultTest) {
  // Reference:
  // camera2/cts/CaptureResultTest.java#testPartialResult
  // Skip the test if partial result is not supported
  if (cam_device_.GetStaticInfo()->GetPartialResultCount() == 1) {
    return;
  }

  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  for (int32_t i = 0; i < num_frames_; i++) {
    ASSERT_EQ(0,
              CreateCaptureRequestByTemplate(CAMERA3_TEMPLATE_PREVIEW, nullptr))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  for (int32_t i = 0; i < num_frames_; i++) {
    WaitShutterAndCaptureResult(timeout);
  }

  ValidatePartialMetadata();
}

// Test parameters:
// - Camera ID
class Camera3ResultTimestampsTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3ResultTimestampsTest() : Camera3FrameFixture(GetParam()) {}

  virtual void SetUp() override;

 protected:
  virtual void Notify(const camera3_notify_msg* msg);

  // Process result metadata and/or output buffers
  virtual void ProcessResultMetadataOutputBuffers(
      uint32_t frame_number,
      CameraMetadataUniquePtr metadata,
      std::vector<BufferHandleUniquePtr>* buffers) override;

  // Validate and get one timestamp
  void ValidateAndGetTimestamp(int64_t* timestamp);

 private:
  // Store timestamps of shutter callback in the first-in-first-out order
  std::list<uint64_t> capture_timestamps_;

  // Store result metadata in the first-in-first-out order
  std::list<CameraMetadataUniquePtr> result_metadata_;
};

void Camera3ResultTimestampsTest::SetUp() {
  Camera3FrameFixture::SetUp();
  cam_device_.RegisterNotifyCallback(
      base::Bind(&Camera3ResultTimestampsTest::Notify, base::Unretained(this)));
}

void Camera3ResultTimestampsTest::Notify(const camera3_notify_msg* msg) {
  VLOGF_ENTER();
  EXPECT_EQ(CAMERA3_MSG_SHUTTER, msg->type)
      << "Shutter error = " << msg->message.error.error_code;

  if (msg->type == CAMERA3_MSG_SHUTTER) {
    capture_timestamps_.push_back(msg->message.shutter.timestamp);
  }
}

void Camera3ResultTimestampsTest::ProcessResultMetadataOutputBuffers(
    uint32_t frame_number,
    CameraMetadataUniquePtr metadata,
    std::vector<BufferHandleUniquePtr>* buffers) {
  VLOGF_ENTER();
  result_metadata_.push_back(std::move(metadata));
}

void Camera3ResultTimestampsTest::ValidateAndGetTimestamp(int64_t* timestamp) {
  ASSERT_FALSE(capture_timestamps_.empty())
      << "Capture timestamp is unavailable";
  ASSERT_FALSE(result_metadata_.empty()) << "Result metadata is unavailable";
  camera_metadata_ro_entry_t entry;
  ASSERT_EQ(0, find_camera_metadata_ro_entry(result_metadata_.front().get(),
                                             ANDROID_SENSOR_TIMESTAMP, &entry))
      << "Metadata key ANDROID_SENSOR_TIMESTAMP not found";
  *timestamp = entry.data.i64[0];
  EXPECT_EQ(capture_timestamps_.front(), *timestamp)
      << "Shutter notification timestamp must be same as capture result"
         " timestamp";
  capture_timestamps_.pop_front();
  result_metadata_.pop_front();
}

TEST_P(Camera3ResultTimestampsTest, GetFrame) {
  // Reference:
  // camera2/cts/CaptureResultTest.java#testResultTimestamps
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";

  ASSERT_EQ(0,
            CreateCaptureRequestByTemplate(CAMERA3_TEMPLATE_PREVIEW, nullptr))
      << "Creating capture request fails";
  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  ASSERT_EQ(0, cam_device_.WaitCaptureResult(timeout));
  int64_t timestamp1 = 0;
  ValidateAndGetTimestamp(&timestamp1);

  ASSERT_EQ(0,
            CreateCaptureRequestByTemplate(CAMERA3_TEMPLATE_PREVIEW, nullptr))
      << "Creating capture request fails";
  GetTimeOfTimeout(1000, &timeout);
  ASSERT_EQ(0, cam_device_.WaitCaptureResult(timeout));
  int64_t timestamp2 = 0;
  ValidateAndGetTimestamp(&timestamp2);

  ASSERT_LT(timestamp1, timestamp2) << "Timestamps must be increasing";
}

// Test parameters:
// - Camera ID
class Camera3InvalidBufferTest : public Camera3FrameFixture,
                                 public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3InvalidBufferTest() : Camera3FrameFixture(GetParam()) {}

  virtual void SetUp() override;

 protected:
  // Callback functions from HAL device
  virtual void ProcessCaptureResult(const camera3_capture_result* result);

  void RunInvalidBufferTest(buffer_handle_t* handle);

  // Callback functions from HAL device
  // Do nothing.
  virtual void Notify(const camera3_notify_msg* msg) {}

  sem_t capture_result_sem_;

 private:
  // Number of configured streams
  static const int32_t kNumberOfConfiguredStreams;
};

const int32_t Camera3InvalidBufferTest::kNumberOfConfiguredStreams = 1;

void Camera3InvalidBufferTest::SetUp() {
  Camera3FrameFixture::SetUp();
  cam_device_.RegisterProcessCaptureResultCallback(base::Bind(
      &Camera3InvalidBufferTest::ProcessCaptureResult, base::Unretained(this)));
  cam_device_.RegisterNotifyCallback(
      base::Bind(&Camera3InvalidBufferTest::Notify, base::Unretained(this)));
  sem_init(&capture_result_sem_, 0, 0);
}

void Camera3InvalidBufferTest::ProcessCaptureResult(
    const camera3_capture_result* result) {
  VLOGF_ENTER();
  ASSERT_NE(nullptr, result) << "Capture result is null";
  for (uint32_t i = 0; i < result->num_output_buffers; i++) {
    EXPECT_EQ(CAMERA3_BUFFER_STATUS_ERROR, result->output_buffers[i].status)
        << "Capture result should return error on invalid buffer";
  }
  if (result->num_output_buffers > 0) {
    sem_post(&capture_result_sem_);
  }
}

void Camera3InvalidBufferTest::RunInvalidBufferTest(buffer_handle_t* handle) {
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  std::vector<const camera3_stream_t*> streams;
  ASSERT_EQ(0, cam_device_.ConfigureStreams(&streams))
      << "Configuring stream fails";
  const camera_metadata_t* default_settings;
  default_settings =
      cam_device_.ConstructDefaultRequestSettings(CAMERA3_TEMPLATE_PREVIEW);
  ASSERT_NE(nullptr, default_settings) << "Camera default settings are NULL";
  camera3_stream_buffer_t stream_buffer = {
      .stream = const_cast<camera3_stream_t*>(streams.front()),
      .buffer = handle,
      .status = CAMERA3_BUFFER_STATUS_OK,
      .acquire_fence = -1,
      .release_fence = -1};
  std::vector<camera3_stream_buffer_t> stream_buffers(1, stream_buffer);
  camera3_capture_request_t capture_request = {
      .frame_number = UINT32_MAX,
      .settings = default_settings,
      .input_buffer = NULL,
      .num_output_buffers = static_cast<uint32_t>(stream_buffers.size()),
      .output_buffers = stream_buffers.data()};
  ASSERT_EQ(0, cam_device_.ProcessCaptureRequest(&capture_request));

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  ASSERT_EQ(0, sem_timedwait(&capture_result_sem_, &timeout));
}

TEST_P(Camera3InvalidBufferTest, NullBufferHandle) {
  buffer_handle_t handle = nullptr;
  RunInvalidBufferTest(&handle);
}

TEST_P(Camera3InvalidBufferTest, FreedBufferHandle) {
  BufferHandleUniquePtr buffer = Camera3TestGralloc::GetInstance()->Allocate(
      default_width_, default_height_, HAL_PIXEL_FORMAT_YCbCr_420_888,
      GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_CAMERA_WRITE);
  buffer_handle_t* handle = buffer.get();
  buffer.reset();
  ASSERT_NE(nullptr, handle);
  RunInvalidBufferTest(handle);
}

INSTANTIATE_TEST_CASE_P(
    Camera3FrameTest,
    Camera3SingleFrameTest,
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
    Camera3FrameTest,
    Camera3MultiFrameTest,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                                         CAMERA3_TEMPLATE_STILL_CAPTURE,
                                         CAMERA3_TEMPLATE_VIDEO_RECORD,
                                         CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
                                         CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
                                         CAMERA3_TEMPLATE_MANUAL),
                       ::testing::Range(1, 10)));

INSTANTIATE_TEST_CASE_P(Camera3FrameTest,
                        Camera3MixedTemplateMultiFrameTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(
    Camera3FrameTest,
    Camera3FlushRequestsTest,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(CAMERA3_TEMPLATE_PREVIEW,
                                         CAMERA3_TEMPLATE_STILL_CAPTURE,
                                         CAMERA3_TEMPLATE_VIDEO_RECORD,
                                         CAMERA3_TEMPLATE_VIDEO_SNAPSHOT,
                                         CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG,
                                         CAMERA3_TEMPLATE_MANUAL),
                       ::testing::Values(10)));

INSTANTIATE_TEST_CASE_P(Camera3FrameTest,
                        Camera3MultiStreamFrameTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(NullOrUnconfiguredRequest,
                        Camera3InvalidRequestTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(
    Camera3FrameTest,
    Camera3SimpleCaptureFrames,
    ::testing::Combine(::testing::ValuesIn(Camera3Module().GetCameraIds()),
                       ::testing::Values(10)));

INSTANTIATE_TEST_CASE_P(Camera3FrameTest,
                        Camera3ResultTimestampsTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

INSTANTIATE_TEST_CASE_P(Camera3FrameTest,
                        Camera3InvalidBufferTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

}  // namespace camera3_test
