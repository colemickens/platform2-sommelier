// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAMERA3_TEST_CAMERA3_STREAM_FIXTURE_H_
#define CAMERA3_TEST_CAMERA3_STREAM_FIXTURE_H_

#include <unordered_map>

#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include <hardware/hardware.h>

#include "camera3_device_fixture.h"

namespace camera3_test {

class ResolutionInfo {
 public:
  ResolutionInfo(int32_t width, int32_t height)
      : width_(width), height_(height) {}

  int32_t Width() const;

  int32_t Height() const;

  int32_t Area() const;

 private:
  int32_t width_, height_;
};

class Camera3StreamFixture : public Camera3DeviceFixture {
 public:
  explicit Camera3StreamFixture(int32_t cam_id)
      : Camera3DeviceFixture(cam_id),
        cam_id_(cam_id),
        default_format_(HAL_PIXEL_FORMAT_YCbCr_420_888),
        default_width_(640),
        default_height_(480) {}

  virtual void SetUp() override;

  virtual void TearDown() override;

  // Select minimal size by number of pixels.
  int32_t GetMinResolution(int32_t format, ResolutionInfo* resolution) const;

  // Select maximal size by number of pixels.
  int32_t GetMaxResolution(int32_t format, ResolutionInfo* resolution) const;

  // Get resolution under limit
  ResolutionInfo CapResolution(ResolutionInfo input,
                               ResolutionInfo limit) const;

 protected:
  const int32_t cam_id_;

  int32_t default_format_;

  int32_t default_width_;

  int32_t default_height_;

 private:
  void BuildOutputResolutions();

  int32_t GetResolutionList(int32_t format,
                            std::vector<ResolutionInfo>* resolutions) const;

  // Available resolutions of output streams
  std::unordered_map<int32_t, std::vector<ResolutionInfo>> output_resolutions_;

  DISALLOW_COPY_AND_ASSIGN(Camera3StreamFixture);
};

}  // namespace camera3_test

#endif  // CAMERA3_TEST_CAMERA3_STREAM_FIXTURE_H_
