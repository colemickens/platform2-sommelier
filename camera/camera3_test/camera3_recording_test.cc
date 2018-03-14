// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "camera3_test/camera3_recording_fixture.h"

#include <string>

#include <base/command_line.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>

namespace camera3_test {

void Camera3RecordingFixture::SetUp() {
  ASSERT_EQ(0, cam_service_.Initialize(
                   Camera3Service::ProcessStillCaptureResultCallback(),
                   base::Bind(&Camera3RecordingFixture::ProcessRecordingResult,
                              base::Unretained(this))))
      << "Failed to initialize camera service";
}

void Camera3RecordingFixture::ProcessRecordingResult(
    int cam_id, uint32_t /*frame_number*/, CameraMetadataUniquePtr metadata) {
  VLOGF_ENTER();
  camera_metadata_ro_entry_t entry;
  ASSERT_EQ(0, find_camera_metadata_ro_entry(metadata.get(),
                                             ANDROID_SENSOR_TIMESTAMP, &entry))
      << "Failed to get sensor timestamp in recording result";
  sensor_timestamp_map_[cam_id].push_back(entry.data.i64[0]);
}

// Test parameters:
// - Camera ID, width, height, frame rate
class Camera3BasicRecordingTest
    : public Camera3RecordingFixture,
      public ::testing::WithParamInterface<
          std::tuple<int32_t, int32_t, int32_t, float>> {
 public:
  const int32_t kRecordingDurationMs = 3000;
  // Margin of frame duration in percetange. The value is adopted from
  // android.hardware.camera2.cts.RecordingTest#testBasicRecording.
  const float kFrameDurationMargin = 20.0;
  // Tolerance of frame drop rate in percetange
  const float kFrameDropRateTolerance = 5.0;

  Camera3BasicRecordingTest()
      : Camera3RecordingFixture(std::vector<int>(1, std::get<0>(GetParam()))),
        cam_id_(std::get<0>(GetParam())),
        recording_resolution_(std::get<1>(GetParam()), std::get<2>(GetParam())),
        recording_frame_rate_(std::get<3>(GetParam())) {}

 protected:
  // |duration_ms|: total duration of recording in milliseconds
  // |frame_duration_ms|: duration of each frame in milliseconds
  void ValidateRecordingFrameRate(float duration_ms, float frame_duration_ms);

  int cam_id_;

  ResolutionInfo recording_resolution_;

  float recording_frame_rate_;
};

void Camera3BasicRecordingTest::ValidateRecordingFrameRate(
    float duration_ms,
    float frame_duration_ms) {
  ASSERT_NE(0, duration_ms);
  ASSERT_NE(0, frame_duration_ms);
  float max_frame_duration_ms =
      frame_duration_ms * (1.0 + kFrameDurationMargin / 100.0);
  float min_frame_duration_ms =
      frame_duration_ms * (1.0 - kFrameDurationMargin / 100.0);
  uint32_t frame_drop_count = 0;
  int64_t prev_timestamp = sensor_timestamp_map_[cam_id_].front();
  sensor_timestamp_map_[cam_id_].pop_front();
  while (!sensor_timestamp_map_[cam_id_].empty()) {
    int64_t cur_timestamp = sensor_timestamp_map_[cam_id_].front();
    sensor_timestamp_map_[cam_id_].pop_front();
    if (static_cast<float>(cur_timestamp - prev_timestamp) / 1000000 >
            max_frame_duration_ms ||
        static_cast<float>(cur_timestamp - prev_timestamp) / 1000000 <
            min_frame_duration_ms) {
      VLOGF(1) << "Frame drop at "
               << (prev_timestamp / 1000000 +
                   static_cast<int64_t>(frame_duration_ms))
               << " ms, actual " << cur_timestamp / 1000000 << " ms";
      ++frame_drop_count;
    }
    prev_timestamp = cur_timestamp;
  }
  float frame_drop_rate =
      100.0 * frame_drop_count * frame_duration_ms / duration_ms;
  ASSERT_LT(frame_drop_rate, kFrameDropRateTolerance)
      << "Camera " << cam_id_
      << " Video frame drop rate too high: " << frame_drop_rate
      << ", tolerance " << kFrameDropRateTolerance;
}

TEST_P(Camera3BasicRecordingTest, BasicRecording) {
#define ARRAY_SIZE(A) (sizeof(A) / sizeof(*(A)))
  ResolutionInfo preview_resolution =
      cam_service_.GetStaticInfo(cam_id_)
          ->GetSortedOutputResolutions(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
          .back();
  ResolutionInfo jpeg_resolution(0, 0);
  cam_service_.StartPreview(cam_id_, preview_resolution, jpeg_resolution,
                            recording_resolution_);

  CameraMetadataUniquePtr recording_metadata(
      clone_camera_metadata(cam_service_.ConstructDefaultRequestSettings(
          cam_id_, CAMERA3_TEMPLATE_VIDEO_RECORD)));
  ASSERT_NE(nullptr, recording_metadata.get());
  int32_t fps_range[] = {static_cast<int32_t>(recording_frame_rate_),
                         static_cast<int32_t>(recording_frame_rate_)};
  EXPECT_EQ(0, UpdateMetadata(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, fps_range,
                              ARRAY_SIZE(fps_range), &recording_metadata));
  cam_service_.StartRecording(cam_id_, recording_metadata.get());
  usleep(kRecordingDurationMs * 1000);
  cam_service_.StopRecording(cam_id_);
  float frame_duration_ms = 1000.0 / recording_frame_rate_;
  float duration_ms = sensor_timestamp_map_[cam_id_].size() * frame_duration_ms;
  ValidateRecordingFrameRate(duration_ms, frame_duration_ms);

  cam_service_.StopPreview(cam_id_);
}

static std::vector<std::tuple<int, int32_t, int32_t, float>>
ParseRecordingParams() {
  // This parameter would be generated and passed by the camera_HAL3 autotest.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch("recording_params")) {
    LOGF(ERROR) << "Missing recording parameters in the test command";
    // Return invalid parameters to fail the test
    return {{-1, 0, 0, 0.0}};
  }
  std::vector<std::tuple<int, int32_t, int32_t, float>> params;
  std::string params_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "recording_params");
  // Expected video recording parameters in the format
  // "camera_id:width:height:frame_rate". For example:
  // "0:1280:720:30,0:1920:1080:30,1:1280:720:30" means camcorder profiles
  // contains 1280x720 and 1920x1080 for camera 0 and just 1280x720 for camera
  // 1.
  const size_t kNumParamsInProfile = 4;
  enum { CAMERA_ID_IDX, WIDTH_IDX, HEIGHT_IDX, FRAME_RATE_IDX };
  for (const auto& it : base::SplitString(
           params_str, ",", base::WhitespaceHandling::TRIM_WHITESPACE,
           base::SplitResult::SPLIT_WANT_ALL)) {
    auto profile =
        base::SplitString(it, ":", base::WhitespaceHandling::TRIM_WHITESPACE,
                          base::SplitResult::SPLIT_WANT_ALL);
    if (profile.size() != kNumParamsInProfile) {
      ADD_FAILURE() << "Failed to parse video recording parameters (" << it
                    << ")";
      continue;
    }
    params.emplace_back(
        std::stoi(profile[CAMERA_ID_IDX]), std::stoi(profile[WIDTH_IDX]),
        std::stoi(profile[HEIGHT_IDX]), std::stof(profile[FRAME_RATE_IDX]));
  }

  std::set<int> param_ids;
  for (const auto& param : params) {
    param_ids.insert(std::get<CAMERA_ID_IDX>(param));
  }

  // We are going to enable usb camera hal on all boards, so there will be more
  // than one hals on many platforms just like today's nautilus.  The
  // recording_params is now generated from media_profiles.xml, where the camera
  // ids are already translated by SuperHAL.  But cros_camera_test is used to
  // test only one camera hal directly without going through the hal_adapter,
  // therefore we have to remap the ids here.
  //
  // TODO(shik): This is a temporary workaround for SuperHAL camera ids mapping
  // until we have better ground truth config file.  Here we exploit the fact
  // that there are at most one back and at most one front internal cameras for
  // now, and all cameras are sorted by facing in SuperHAL.  I feel bad when
  // implementing the following hack (sigh).
  std::vector<std::tuple<int, int32_t, int32_t, float>> result;
  auto cam_ids = Camera3Module().GetCameraIds();
  if (cam_ids.size() < param_ids.size()) {
    // SuperHAL case
    for (const auto& cam_id : cam_ids) {
      camera_info info;
      EXPECT_EQ(0, Camera3Module().GetCameraInfo(cam_id, &info));
      bool found_matching_param = false;
      for (auto param : params) {
        if (std::get<CAMERA_ID_IDX>(param) == info.facing) {
          found_matching_param = true;
          std::get<CAMERA_ID_IDX>(param) = cam_id;
          result.emplace_back(param);
        }
      }
      EXPECT_TRUE(found_matching_param);
    }
  } else {
    // Single HAL case
    for (const auto& cam_id : cam_ids) {
      if (std::find_if(
              params.begin(), params.end(),
              [&](const std::tuple<int, int32_t, int32_t, float>& item) {
                return std::get<CAMERA_ID_IDX>(item) == cam_id;
              }) == params.end()) {
        ADD_FAILURE() << "Missing video recording parameters for camera "
                      << cam_id;
      }
    }
    result = std::move(params);
  }

  LOGF(INFO) << "The parameters will be used for recording test:";
  for (const auto& param : result) {
    LOGF(INFO) << base::StringPrintf(
        "camera id = %d, size = %dx%d, fps = %g",
        std::get<CAMERA_ID_IDX>(param), std::get<WIDTH_IDX>(param),
        std::get<HEIGHT_IDX>(param), std::get<FRAME_RATE_IDX>(param));
  }

  return result;
}

INSTANTIATE_TEST_CASE_P(Camera3RecordingFixture,
                        Camera3BasicRecordingTest,
                        ::testing::ValuesIn(ParseRecordingParams()));

}  // namespace camera3_test
