// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_frame_fixture.h"

#include "base/macros.h"
#include <sync/sync.h>

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

  num_partial_results_ = cam_device_.GetStaticInfo()->GetPartialResultCount();
}

void Camera3FrameFixture::TearDown() {
  sem_destroy(&shutter_sem_);
  sem_destroy(&capture_result_sem_);

  Camera3StreamFixture::TearDown();
}

int32_t Camera3FrameFixture::CreateCaptureRequest(int32_t type,
                                                  uint32_t* frame_number) {
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

  if (frame_number) {
    *frame_number = request_frame_number_;
  }
  request_frame_number_++;

  return ret;
}

void Camera3FrameFixture::WaitCaptureResult(const struct timespec& timeout) {
  // Wait for shutter callback
  ASSERT_EQ(0, sem_timedwait(&shutter_sem_, &timeout));

  // Wait for capture result callback
  ASSERT_EQ(0, sem_timedwait(&capture_result_sem_, &timeout));
}

bool Camera3FrameFixture::ProcessPartialResult(
    const camera3_capture_result& result) {
  // True if this partial result is the final one. If HAL does not use partial
  // result, the value is True by default.
  bool is_final_partial_result = !UsePartialResult();
  // Check if this result carries only partial metadata
  if (UsePartialResult() && result.result != NULL) {
    EXPECT_LE(result.partial_result, num_partial_results_);
    EXPECT_GE(result.partial_result, 1);
    camera_metadata_ro_entry_t entry;
    if (find_camera_metadata_ro_entry(
            result.result, ANDROID_QUIRKS_PARTIAL_RESULT, &entry) == 0) {
      is_final_partial_result =
          (entry.data.i32[0] == ANDROID_QUIRKS_PARTIAL_RESULT_FINAL);
    }
  }

  // Did we get the (final) result metadata for this capture?
  if (result.result != NULL && is_final_partial_result) {
    EXPECT_FALSE(capture_result_[result.frame_number].have_result_metadata_)
        << "Called multiple times with final metadata";
    capture_result_[result.frame_number].have_result_metadata_ = true;
  }

  capture_result_[result.frame_number].AllocateAndCopyMetadata(*result.result);
  return is_final_partial_result;
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
  ASSERT_NE(capture_request_.end(), capture_request_.find(result->frame_number))
      << "A result is received for nonexistent request";

  // For HAL3.2 or above, If HAL doesn't support partial, it must always set
  // partial_result to 1 when metadata is included in this result.
  ASSERT_TRUE(UsePartialResult() || result->result == NULL ||
              result->partial_result == 1)
      << "Result is malformed: partial_result must be 1 if partial result is "
         "not supported";

  if (result->result) {
    ProcessPartialResult(*result);
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

  capture_result_[result->frame_number].num_output_buffers_ +=
      result->num_output_buffers;
  ASSERT_LE(capture_result_[result->frame_number].num_output_buffers_,
            capture_request_[result->frame_number].num_output_buffers);
  if (capture_result_[result->frame_number].num_output_buffers_ ==
          capture_request_[result->frame_number].num_output_buffers &&
      capture_result_[result->frame_number].have_result_metadata_) {
    ASSERT_EQ(completed_request_.end(),
              completed_request_.find(result->frame_number))
        << "Multiple results are received for the same request";
    completed_request_.insert(result->frame_number);

    ASSERT_EQ(result->frame_number, result_frame_number_)
        << "Capture result is out of order";
    result_frame_number_++;

    // Everything looks fine. Post it now.
    sem_post(&capture_result_sem_);
  }
}

void Camera3FrameFixture::Notify(const camera3_notify_msg* msg) {
  EXPECT_EQ(CAMERA3_MSG_SHUTTER, msg->type) << "Shutter error = "
                                            << msg->message.error.error_code;

  if (msg->type == CAMERA3_MSG_SHUTTER) {
    capture_timestamps_.push_back(msg->message.shutter.timestamp);
    sem_post(&shutter_sem_);
  }
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

void Camera3FrameFixture::ValidateCaptureResultKeys() {
  std::set<int32_t> waiver_keys;
  GetWaiverKeys(&waiver_keys);
  for (auto& request : capture_request_) {
    uint32_t frame_number = request.first;
    if (capture_result_.find(frame_number) == capture_result_.end()) {
      ADD_FAILURE() << "Result is not received for capture request "
                    << frame_number;
      continue;
    }
    CaptureResultInfo* result = &capture_result_[frame_number];

    for (auto key : capture_result_keys) {
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
          camera_metadata_ro_entry_t entry;
          if (find_camera_metadata_ro_entry(request.second.settings, key,
                                            &entry)) {
            ADD_FAILURE() << "Metadata " << get_camera_metadata_tag_name(key)
                          << " is unavailable in capture request";
            continue;
          }
          EXPECT_EQ(entry.data.i32[0], result->GetMetadataKeyValue(key))
              << "Wrong value of metadata " << get_camera_metadata_tag_name(key)
              << " in capture result";
          break;
        case ANDROID_REQUEST_PIPELINE_DEPTH:
          break;
        default:
          // Only do non-null check for the rest of keys.
          EXPECT_TRUE(result->IsMetadataKeyAvailable(key))
              << "Metadata " << get_camera_metadata_tag_name(key)
              << " is unavailable in capture result";
          break;
      }
    }
  }
}

void Camera3FrameFixture::GetWaiverKeys(std::set<int32_t>* waiver_keys) const {
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

bool Camera3FrameFixture::UsePartialResult() const {
  return num_partial_results_ > 1;
}

void Camera3FrameFixture::ValidatePartialResults() {
  for (auto& it : capture_request_) {
    uint32_t frame_number = it.first;
    if (capture_result_.find(frame_number) == capture_result_.end()) {
      ADD_FAILURE() << "Result is not received for capture request "
                    << frame_number;
      continue;
    }
    const CaptureResultInfo* result = &capture_result_[frame_number];

    // Number of partial results is less than or equal to
    // REQUEST_PARTIAL_RESULT_COUNT
    EXPECT_GE(num_partial_results_, result->partial_metadata_.size())
        << "Number of received partial results is wrong";

    // Each key appeared in partial results must be unique across all partial
    // results
    for (size_t i = 0; i < result->partial_metadata_.size(); i++) {
      size_t entry_count =
          get_camera_metadata_entry_count(result->partial_metadata_[i].get());
      for (size_t entry_index = 0; entry_index < entry_count; entry_index++) {
        camera_metadata_ro_entry_t entry;
        ASSERT_EQ(
            0, get_camera_metadata_ro_entry(result->partial_metadata_[i].get(),
                                            entry_index, &entry));
        int32_t key = entry.tag;
        for (size_t j = i + 1; j < result->partial_metadata_.size(); j++) {
          camera_metadata_ro_entry_t entry;
          EXPECT_NE(0, find_camera_metadata_ro_entry(
                           result->partial_metadata_[j].get(), key, &entry))
              << "Key " << get_camera_metadata_tag_name(key)
              << " appears in multiple partial results";
        }
      }
    }
  }
}

void Camera3FrameFixture::ValidateAndGetTimestamp(uint32_t frame_number,
                                                  int64_t* timestamp) {
  ASSERT_NE(capture_result_.end(), capture_result_.find(frame_number))
      << "Result is not received for capture request " << frame_number;
  ASSERT_FALSE(capture_timestamps_.empty())
      << "Capture timestamp is unavailable";
  *timestamp = capture_result_[frame_number].GetMetadataKeyValue64(
      ANDROID_SENSOR_TIMESTAMP);
  EXPECT_EQ(capture_timestamps_.front(), *timestamp)
      << "Shutter notification timestamp must be same as capture result"
         " timestamp";
  capture_timestamps_.pop_front();
}

void Camera3FrameFixture::CaptureResultInfo::AllocateAndCopyMetadata(
    const camera_metadata_t& src) {
  CameraMetadataUniquePtr metadata(allocate_copy_camera_metadata_checked(
      &src, get_camera_metadata_size(&src)));
  ASSERT_NE(nullptr, metadata.get()) << "Copying result partial metadata fails";
  partial_metadata_.push_back(std::move(metadata));
}

bool Camera3FrameFixture::CaptureResultInfo::IsMetadataKeyAvailable(
    int32_t key) const {
  return GetMetadataKeyEntry(key, nullptr);
}

int32_t Camera3FrameFixture::CaptureResultInfo::GetMetadataKeyValue(
    int32_t key) const {
  camera_metadata_ro_entry_t entry;
  return GetMetadataKeyEntry(key, &entry) ? entry.data.i32[0] : -EINVAL;
}

int64_t Camera3FrameFixture::CaptureResultInfo::GetMetadataKeyValue64(
    int32_t key) const {
  camera_metadata_ro_entry_t entry;
  return GetMetadataKeyEntry(key, &entry) ? entry.data.i64[0] : -EINVAL;
}

bool Camera3FrameFixture::CaptureResultInfo::GetMetadataKeyEntry(
    int32_t key,
    camera_metadata_ro_entry_t* entry) const {
  camera_metadata_ro_entry_t local_entry;
  entry = entry ? entry : &local_entry;
  for (const auto& it : partial_metadata_) {
    if (find_camera_metadata_ro_entry(it.get(), key, entry) == 0) {
      return true;
    }
  }
  return false;
}

const int32_t Camera3FrameFixture::capture_result_keys[] = {
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

    ASSERT_EQ(0, CreateCaptureRequest(type, nullptr))
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
    EXPECT_EQ(0, CreateCaptureRequest(type, nullptr))
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
    EXPECT_EQ(0, CreateCaptureRequest(types[i], nullptr))
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
  Camera3FlushRequestsTest()
      : Camera3FrameFixture(std::get<0>(GetParam())), num_capture_results_(0) {}

 protected:
  // Callback functions from HAL device
  virtual void ProcessCaptureResult(
      const camera3_capture_result* result) override;

  // Callback functions from HAL device
  // Do nothing.
  virtual void Notify(const camera3_notify_msg* msg) override {}

  // Number of received capture results with all output buffers returned
  int32_t num_capture_results_;

 private:
  // Number of configured streams
  static const int32_t num_configured_stream_;

  // Store number of output buffers returned in capture results with frame
  // number as the key
  std::unordered_map<uint32_t, int32_t> num_capture_result_buffers_;
};

const int32_t Camera3FlushRequestsTest::num_configured_stream_ = 1;

void Camera3FlushRequestsTest::ProcessCaptureResult(
    const camera3_capture_result* result) {
  ASSERT_NE(nullptr, result) << "Capture result is null";
  num_capture_result_buffers_[result->frame_number] +=
      result->num_output_buffers;
  if (num_capture_result_buffers_[result->frame_number] ==
      num_configured_stream_) {
    num_capture_results_++;
  }
}

TEST_P(Camera3FlushRequestsTest, GetFrame) {
  // The number of configured streams must match the value of
  // |num_configured_stream_|.
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

  int32_t type = std::get<1>(GetParam());
  if (!cam_device_.IsTemplateSupported(type)) {
    return;
  }

  const int32_t num_frames = std::get<2>(GetParam());
  for (int32_t i = 0; i < num_frames; i++) {
    EXPECT_EQ(0, CreateCaptureRequest(type, nullptr))
        << "Creating capture request fails";
  }

  ASSERT_EQ(0, cam_device_.Flush()) << "Flushing capture requests fails";

  // flush() should only return when there are no more outstanding buffers or
  // requests left in the HAL
  EXPECT_EQ(num_frames, num_capture_results_)
      << "There are requests left in the HAL after flushing";
  cam_device_.ClearOutputStreamBuffers();
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

  ASSERT_EQ(0, CreateCaptureRequest(CAMERA3_TEMPLATE_PREVIEW, nullptr))
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

// Test parameters:
// - Camera ID
// - Number of frames to capture
class Camera3SimpleCaptureFrames
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<std::tuple<int32_t, int32_t>> {
 public:
  Camera3SimpleCaptureFrames()
      : Camera3FrameFixture(std::get<0>(GetParam())),
        num_frames(std::get<1>(GetParam())) {}

  const int32_t num_frames;
};

TEST_P(Camera3SimpleCaptureFrames, Camera3ResultAllKeysTest) {
  // Reference:
  // camera2/cts/CaptureResultTest.java#testCameraCaptureResultAllKeys
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

  for (int32_t i = 0; i < num_frames; i++) {
    ASSERT_EQ(0, CreateCaptureRequest(CAMERA3_TEMPLATE_PREVIEW, nullptr))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  for (int32_t i = 0; i < num_frames; i++) {
    WaitCaptureResult(timeout);
  }

  ValidateCaptureResultKeys();
}

TEST_P(Camera3SimpleCaptureFrames, Camera3PartialResultTest) {
  // Reference:
  // camera2/cts/CaptureResultTest.java#testPartialResult
  // Skip the test if partial result is not supported
  if (!UsePartialResult()) {
    return;
  }

  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

  for (int32_t i = 0; i < num_frames; i++) {
    ASSERT_EQ(0, CreateCaptureRequest(CAMERA3_TEMPLATE_PREVIEW, nullptr))
        << "Creating capture request fails";
  }

  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  for (int32_t i = 0; i < num_frames; i++) {
    WaitCaptureResult(timeout);
  }

  ValidatePartialResults();
}

// Test parameters:
// - Camera ID
class Camera3ResultTimestampsTest
    : public Camera3FrameFixture,
      public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3ResultTimestampsTest() : Camera3FrameFixture(GetParam()) {}
};

TEST_P(Camera3ResultTimestampsTest, GetFrame) {
  // Reference:
  // camera2/cts/CaptureResultTest.java#testResultTimestamps
  cam_device_.AddOutputStream(default_format_, default_width_, default_height_);
  ASSERT_EQ(0, cam_device_.ConfigureStreams()) << "Configuring stream fails";

  uint32_t frame_number;
  ASSERT_EQ(0, CreateCaptureRequest(CAMERA3_TEMPLATE_PREVIEW, &frame_number))
      << "Creating capture request fails";
  struct timespec timeout;
  GetTimeOfTimeout(1000, &timeout);
  WaitCaptureResult(timeout);
  int64_t timestamp1 = 0;
  ValidateAndGetTimestamp(frame_number, &timestamp1);

  ASSERT_EQ(0, CreateCaptureRequest(CAMERA3_TEMPLATE_PREVIEW, &frame_number))
      << "Creating capture request fails";
  GetTimeOfTimeout(1000, &timeout);
  WaitCaptureResult(timeout);
  int64_t timestamp2 = 0;
  ValidateAndGetTimestamp(frame_number, &timestamp2);

  ASSERT_LT(timestamp1, timestamp2) << "Timestamps must be increasing";
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

}  // namespace camera3_test
