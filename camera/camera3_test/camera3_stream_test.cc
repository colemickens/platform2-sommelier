// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "camera3_stream_fixture.h"

namespace camera3_test {

void Camera3StreamFixture::SetUp() {
  Camera3DeviceFixture::SetUp();

  std::vector<ResolutionInfo> resolutions =
      cam_device_.GetStaticInfo()->GetSortedOutputResolutions(
          HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
  ASSERT_TRUE(!resolutions.empty())
      << "Failed to find resolutions for implementation defined format";

  default_width_ = resolutions[0].Width();
  default_height_ = resolutions[0].Height();
}

void Camera3StreamFixture::TearDown() {
  Camera3DeviceFixture::TearDown();
}

int32_t Camera3StreamFixture::GetMinResolution(int32_t format,
                                               ResolutionInfo* resolution) {
  if (!resolution) {
    return -EINVAL;
  }

  std::vector<ResolutionInfo> resolutions =
      cam_device_.GetStaticInfo()->GetSortedOutputResolutions(format);
  if (resolutions.empty()) {
    return -EIO;
  }

  ResolutionInfo min_resolution(resolutions[0]);
  for (auto& it : resolutions) {
    if (it.Area() < min_resolution.Area()) {
      min_resolution = it;
    }
  }
  *resolution = min_resolution;
  return 0;
}

int32_t Camera3StreamFixture::GetMaxResolution(int32_t format,
                                               ResolutionInfo* resolution) {
  if (!resolution) {
    return -EINVAL;
  }

  std::vector<ResolutionInfo> resolutions =
      cam_device_.GetStaticInfo()->GetSortedOutputResolutions(format);
  if (resolutions.empty()) {
    return -EIO;
  }

  ResolutionInfo max_resolution(resolutions[0]);
  for (auto& it : resolutions) {
    if (it.Area() > max_resolution.Area()) {
      max_resolution = it;
    }
  }
  *resolution = max_resolution;
  return 0;
}

ResolutionInfo Camera3StreamFixture::CapResolution(ResolutionInfo input,
                                                   ResolutionInfo limit) const {
  if (input.Area() > limit.Area()) {
    return limit;
  }
  return input;
}

// Test spec:
// - Camera ID
// - Output stream format
class Camera3StreamTest
    : public Camera3StreamFixture,
      public ::testing::WithParamInterface<std::tuple<int32_t, int32_t>> {
 public:
  Camera3StreamTest() : Camera3StreamFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3StreamTest, CreateStream) {
  int32_t format = std::get<1>(GetParam());

  cam_device_.AddOutputStream(format, default_width_, default_height_);
  if (cam_device_.GetStaticInfo()->IsFormatAvailable(format)) {
    ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
        << "Configuring stream of supported format fails";
  } else {
    ASSERT_NE(0, cam_device_.ConfigureStreams(nullptr))
        << "Configuring stream of unsupported format succeeds";
  }
}

// Test spec:
// - Camera ID
// - Output stream format
class Camera3BadResultionStreamTest
    : public Camera3StreamFixture,
      public ::testing::WithParamInterface<std::tuple<int32_t, int32_t>> {
 public:
  Camera3BadResultionStreamTest()
      : Camera3StreamFixture(std::get<0>(GetParam())) {}
};

TEST_P(Camera3BadResultionStreamTest, CreateStream) {
  int32_t format = std::get<1>(GetParam());

  if (cam_device_.GetStaticInfo()->IsFormatAvailable(format)) {
    int32_t bad_width = default_width_ + 1;
    std::vector<ResolutionInfo> available_resolutions =
        cam_device_.GetStaticInfo()->GetSortedOutputResolutions(format);
    while (std::find(available_resolutions.begin(), available_resolutions.end(),
                     ResolutionInfo(bad_width, default_height_)) !=
           available_resolutions.end()) {
      bad_width++;
    }
    cam_device_.AddOutputStream(format, bad_width, default_height_);
    ASSERT_NE(0, cam_device_.ConfigureStreams(nullptr))
        << "Configuring stream of bad resolution succeeds";
  }
}

// Test spec:
// - Camera ID
class Camera3MultiStreamTest : public Camera3StreamFixture,
                               public ::testing::WithParamInterface<int32_t> {
 public:
  Camera3MultiStreamTest() : Camera3StreamFixture(GetParam()) {}
};

TEST_P(Camera3MultiStreamTest, CreateStream) {
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

  ASSERT_EQ(0, cam_device_.ConfigureStreams(nullptr))
      << "Configuring stream fails";
}

INSTANTIATE_TEST_CASE_P(
    CreateStream,
    Camera3StreamTest,
    ::testing::Combine(
        ::testing::ValuesIn(Camera3Module().GetCameraIds()),
        ::testing::Values(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                          HAL_PIXEL_FORMAT_YCbCr_420_888,
                          HAL_PIXEL_FORMAT_YCrCb_420_SP,
                          HAL_PIXEL_FORMAT_BLOB,
                          HAL_PIXEL_FORMAT_YV12,
                          HAL_PIXEL_FORMAT_Y8,
                          HAL_PIXEL_FORMAT_Y16,
                          HAL_PIXEL_FORMAT_RAW16)));

INSTANTIATE_TEST_CASE_P(
    CreateStream,
    Camera3BadResultionStreamTest,
    ::testing::Combine(
        ::testing::ValuesIn(Camera3Module().GetCameraIds()),
        ::testing::Values(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
                          HAL_PIXEL_FORMAT_YCbCr_420_888,
                          HAL_PIXEL_FORMAT_YCrCb_420_SP,
                          HAL_PIXEL_FORMAT_BLOB,
                          HAL_PIXEL_FORMAT_YV12,
                          HAL_PIXEL_FORMAT_Y8,
                          HAL_PIXEL_FORMAT_Y16,
                          HAL_PIXEL_FORMAT_RAW16)));

INSTANTIATE_TEST_CASE_P(CreateStream,
                        Camera3MultiStreamTest,
                        ::testing::ValuesIn(Camera3Module().GetCameraIds()));

}  // namespace camera3_test
